// s64chan.cpp
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
#include "s64priv.h"
#include "s64chan.h"
#include <iostream>

using namespace ceds64;
using namespace std;

//----------------TDiskLookup ----------------------------------------------
// This copy constructor is here to avoid copy of unused elements
TDiskLookup::TDiskLookup(const TDiskLookup& rhs)
    : TDiskBlockHead( rhs )
{
    assert(m_nItems <= DLUItems);
    std::copy_n(rhs.m_items.begin(), rhs.m_nItems, m_items.begin());
    std::fill(m_items.begin()+m_nItems, m_items.end(), TDiskTableItem(0,0));
#ifdef _DEBUG
    if (!Verify())
        Dump();
#endif
}

TDiskLookup::TDiskLookup()
    : TDiskBlockHead()
{
}

//! Add an item to the index table.
/*!
 Attempt to add the position and time of a block into a TDiskLookup object.
 \param pos     The position in the file of the added block
 \param time    The time of the first item in the added block
 \return        The index (0..DLUItems-1) at which the item was added to the table or
                DLUItems if the table is already full or a negative error code, being
                CORRUPT_FILE if time is not greater than the last item in the lookup.

 It turns out that added data should be at a position that is after previous data. We
 do not (currently) check for this, but it does seem to be a consequence of how the data
 is written, and we could enforce this (here) if we wanted to. Having items in physical
 disk order does help when attempting to fix a damaged file.
*/
int TDiskLookup::AddIndexItem(TDiskOff pos, TSTime64 time)
{
    if (m_nItems == DLUItems)           // If it is full...
        return DLUItems;                // ...so cannot add
    if (m_nItems > 0)                   // if already got items
    {
        if (time <= m_items[m_nItems-1].m_time)
        {
            assert(false);              // if debugging, warn of serious error
            return CORRUPT_FILE;
        }
        assert(m_items[m_nItems-1].m_do < pos);
    }
    m_items[m_nItems].m_do = pos;       // set new values
    m_items[m_nItems].m_time = time;
    ++m_nItems;
    return m_nItems-1;
}

//! Implement the upper_bound algorithm to locate an index
/*!
 Search the table for the index where a specified time would be placed.
 \param time    The time we wish to locate
 \param nUse    If non-zero, it must be <= m_nItems and is the number of items to search.
                This is for the case when we are reusing a block, so all items might not
                (yet) be used.
 \return        The index of the first item that starts at a time that is > time, or
                returns m_nItems if no item is greater. A value of 0 means that the
                first item is at a greater time.
*/
unsigned int TDiskLookup::UpperBound(TSTime64 time, size_t nUse) const
{
    assert (nUse <= m_nItems);      // detect a logic error
    size_t nIndex = nUse ? nUse : m_nItems;
    auto it = std::upper_bound(m_items.begin(), m_items.begin()+nIndex, TDiskTableItem(time));
    return static_cast<unsigned int>(it - m_items.begin());    // index is 0 to nIndex
}

// Now a member of TDiskTableItem
//bool ceds64::operator<(const TDiskTableItem& lhs, const TDiskTableItem& rhs)
//{
//    return lhs.m_time < rhs.m_time;
//}

//! Is the data consistent
/*!
 This checks that the item level is possible, that the items are possible and that
 both the time and disk offset of each item increases with each entry.
 \return true if all seems OK, false if not.
*/
bool TDiskLookup::Verify() const
{
    if (/*(GetLevel() == 0) ||*/ (GetLevel() > 7))
        return false;
    if (/*(m_nItems == 0) ||*/ (m_nItems > DLUItems))
        return false;
    for (size_t i = 0; i<m_nItems; ++i)
    {
        if (m_items[i].m_do == 0)
            return false;
        if (i > 0)
        {
            if (m_items[i].m_time <= m_items[i-1].m_time)
                return false;
            if (m_items[i].m_do <= m_items[i-1].m_do)
                return false;
        }
    }
    return true;
}

#ifdef _DEBUG
//! Debugging routine used to display the look up table
/*!
 This is only used by the library when committing and Verify fails on a block.
*/
void TDiskLookup::Dump() const
{
    cout << hex << "TDiskLookup dump\n";
    cout << "Parent: " << GetParentBlock() << "Index: " << GetParentIndex() << "Level: " << GetLevel() << '\n';
    cout << dec << "Items : " << m_nItems << " (time, disk)\n";
    for (size_t i=0; i<m_nItems; ++i)
        cout << i << ", " << m_items[i].m_time << ", " << m_items[i].m_do << '\n';
}
#endif

//-----------------CIndex ---------------------------------------------------
// Holder for index blocks used to locate data

//! Constructor setting up a new block and initialising the TDiskLookup
/*!
\param chan     The channel number this belongs to.
\param doParent The disk address of the parent block. The bottom bits of this must be 0
                as they are used to encode level and parent index.
\param index    The index into the parent block that points at this block.
\param level    The level of this block (1 upwards as level 0 is a data block).
*/
CIndex::CIndex(TChanNum chan, TDiskOff doParent, unsigned int index, unsigned int level)
    : m_do( 0 )
    , m_bModified( false )
    , m_indexReuse( 0 )
{
    m_dlu.Init(doParent, chan);
    m_dlu.SetParent(doParent, index, level);
}

//! Set where the index block is stored on disk
/*!
\param pos  The disk offset to the storage location.
*/
void CIndex::SetDiskOffset(TDiskOff pos)
{
    m_do = pos;
}

//! Add a new index item to the table.
/*!
\param pos  The disk position of the block to add.
\param time The time of the first item in the block to be added.
\return     The index in the table of the added item, or DLUItems if this index is full,
            or a negative error code. DLUItems means that the caller must take action
            to allocate a new block (or blocks).
*/
int CIndex::AddIndexItem(TDiskOff pos, TSTime64 time)
{
    int err = m_dlu.AddIndexItem(pos, time);
    if ((err >= 0) && (err < DLUItems))
        m_bModified = true;
    return err;
}

//! Fill in the parent information in the TDiskLookup block
/*!
\param doParent The disk address of the parent block. The bottom bits of this must be 0
                as they are used to encode level and parent index.
\param index    The index into the parent block that points at this block.
\param level    The level of this block (1 upwards as level 0 is a data block).
*/
void CIndex::SetParent(TDiskOff doParent, unsigned int index, unsigned int level)
{
    m_dlu.SetParent(doParent, index, level);
    m_bModified = true;
}

//! Get the time of the first item in the data block (the block MUST NOT be empty)
/*!
\return The first item time. If the block is empty, this will be rubbish.
*/
TSTime64 CIndex::GetFirstTime() const
{
    assert(m_dlu.m_nItems > 0);
    return m_dlu.m_items[0].m_time;
}

//! Clean up a previously used block.
/*!
m_bModified should have been cleared before this call, meaning that if the block needed
to be written before being reused, this has been done. The channel number is not changed.
*/
void CIndex::clear()
{
    assert(!m_bModified);           // It should not be modified
    m_dlu.Init(0, m_dlu.m_chan);
    m_do = 0;
    m_bModified = false;
}

//! Increment the reuse index.
/*!
When we reuse a channel, we start by resuing the previously allocated blocks. In
this case, all the indeices for a channel except the very last, must be full.
If the index i already at the end, we set it to 0 and return true. Otherwise
we increment it (and assert if it has gone neyond the data). We rely on the caller
to handle what happens in the final index block where m_dlu.m_nItems may be less
than DLUItems.
\return true if incrementing the reuse index has wrapped around to 0, else false.
*/
bool CIndex::IncReuseIndex()
{
    if (m_indexReuse == DLUItems-1)
    {
        m_indexReuse = 0;
        return true;
    }

    ++m_indexReuse;					// on to the next item
    assert(m_indexReuse < m_dlu.m_nItems);
    return false;
}

//! Set the time of the current reuse item
/*!
When we reuse indices, the disk positions all stay the same, but the times are
changed. If the time is changed we mark the block modified.
\param tFirst The time of the current reuse index item.
*/
void CIndex::SetReuseTime(TSTime64 tFirst)
{
    assert(m_indexReuse < m_dlu.m_nItems);
    assert(!m_indexReuse || (m_dlu.m_items[m_indexReuse-1].m_time < tFirst));
    if (m_dlu.m_items[m_indexReuse].m_time != tFirst)
    {
        m_dlu.m_items[m_indexReuse].m_time = tFirst;
        m_bModified = true;         // We have made a change, must rewrite
    }
}

//-----------------TChanHeader ----------------------------------------------
// Constructor used when adding channels to the vector of channels.
TChanHead::TChanHead()
    : m_doIndex( 0 )            // no channel index block
    , m_lastTime( -1 )          // impossible last time
    , m_nBlocks( 0 )
    , m_nAllocatedBlocks( 0 )
    , m_nObjSize( 0 )
    , m_nRows( 0 )
    , m_nColumns( 0 )
    , m_nPreTrig( 0 )
    , m_nItemSize( 0 )
    , m_chanID( 0 )
    , m_chanKind( ChanOff )
    , m_lastKind( ChanOff )
    , m_iPhyCh( -1 )
    , m_title( 0 )
    , m_units( 0 )
    , m_comment( 0 )
    , m_tDivide( 0 )
    , m_dRate( 0.0 )
    , m_dScale( 1.0 )
    , m_dOffset( 0.0 )
    , m_dYLow( -1.0 )
    , m_dYHigh( 1.0 )
    , m_flags( 0 )
{
    m_pad.fill(0);              // fill in the padding (added Feb, 2015)
}

//! Empty the channel head for reuse
/*!
We want to reuse a channel, preserving all the current state except to state that the
channel now has no data. If the channel had any data we increment m_chanID.
*/
void TChanHead::EmptyForReuse()
{
    if (m_nAllocatedBlocks == 0)			// if we had any blocks in use...
        m_nAllocatedBlocks = m_nBlocks;		// ...we will now reuse them
    m_nBlocks = 0;							// the channel has no blocks
    if (m_nAllocatedBlocks)					// if reusing the channel...
        ++m_chanID;							// ...move ID on to aid recovery

    m_lastTime = -1;						// no data in the channel, no last time
}

//! Reset a previously used channel for reuse
/*!
The caller of this must have released any strings. We
 set the channel to have no blocks, passing any blocks to the allocated list. We reset
 all the channel header information and set the channel to be ChanOff.
*/
void TChanHead::ResetForReuse()
{
    assert(m_chanKind == ChanOff);          // channel assumed not in use
    assert(m_lastKind != ChanOff);          // and we have a last kind
    EmptyForReuse();                        // prepare allocated channel blocks for reuse
    m_nObjSize = 0;							// we have nothing stored yet
    m_nRows = m_nColumns = m_nItemSize = 0;	// no attached repeated things
    m_chanKind = m_lastKind = ChanOff;      // no memory of previous use
    m_iPhyCh = -1;							// no channel
    m_title = m_units = m_comment = 0;		// caller should have released any strings
    m_tDivide = 0;							// time per value for wave, Wavemark
    m_dRate = 0.0;							// ideal rate
}

//! Increment blocks used when resusing blocks
/*!
Increment the block count. If this reaches the count of allocated blocks, then
we cancel the use of allocated blocks by setting the allocated count back to 0.
You should ONLY call this if ReusingBlocks() has just returned true.
*/
void TChanHead::IncReusedBlocks()
{
    if (++m_nBlocks == m_nAllocatedBlocks)  // inc count, see if this exhausts reuse
        m_nAllocatedBlocks = 0;	            // no longer re-using blocks
}

//! Undelete a previously deleted channel
/*!
 If this is a deleted channel we can undelete it. All we have to do is to restore the
 channel type.
*/
int TChanHead::Undelete()
{
    if (!IsDeleted())                       // check we are in the correct state
        return CHANNEL_TYPE;
    m_chanKind = m_lastKind;				// restore the deleted channel
    m_lastKind = ChanOff;					// this is just to be tidy
    return 0;
}

//! Delete a used channel.
/*!
 If a channel is in use we can delete it in such a way that it can be undeleted. All we
 do is save the channel type, then set the type to ChanOff.
*/
bool TChanHead::Delete()
{
    if (IsUsed())                           // Delete of unused channel is not an error
    {
        m_lastKind = m_chanKind;            // save the type
        m_chanKind = ChanOff;               // channel is deleted
        return true;
    }
    return false;
}

//-------------------CSon64Chan ----------------------------------------------
// Base type for channels

/*!
Constructor. We should really have two constructors. One for the case where we are
opening a file and the channel head already holds the contents read from disk, and the
other for the case where we are creating a new channel.
\param file  The file that this channel belongs to.
\param nChan The channel number.
\param kind  The channel type.
*/
CSon64Chan::CSon64Chan(TSon64File& file, TChanNum nChan, TDataKind kind)
    : m_file( file)
    , m_nChan( nChan )
    , m_bmRead( *this )                     // Warning: ctor must not USE
    , m_chanHead( file.ChanHead(nChan) )    // copy channel head from file
    , m_bModified( kind != m_chanHead.m_chanKind )  // we are setting things
{
    assert(kind != ChanOff);
    bool bWasInUse = m_chanHead.m_lastKind != ChanOff;
    if (bWasInUse)                          // If the channel was in use...
        ResetForReuse();                    // ...let it go
    m_chanHead.m_chanKind = kind;

    // Insist that the strings exist or are empty
    if (!file.m_ss.Verify(m_chanHead.m_title))
        m_chanHead.m_title = 0;             // This is an error, but fix it
    if (!file.m_ss.Verify(m_chanHead.m_units))  
        m_chanHead.m_units = 0;             // This is an error, but fix it
    if (!file.m_ss.Verify(m_chanHead.m_comment))
        m_chanHead.m_comment = 0;           // This is an error, but fix it
    if (bWasInUse)                          // If was in use...
        FixIndex();                         // ...check old index OK
}

CSon64Chan::~CSon64Chan()
{
}

//------------------------- Delete and Undelete ------------------------------

//! Delete a channel leaving it in a state where it can be undeleted
/*!
You _must not_ hold the channel mutex.
We want to delete a channel. Deletion means marking the channel as no longer
existing, BUT, we can still recover it as all the disk data is still in place.
You cannot recover the original if the channel is re-used, of course.
\return This currently always returns 0. Deleting a deleted channel is not an
        error and has no effect.
*/
int CSon64Chan::Delete()
{
    int err = Commit();                 // commit the original state
    if (err == 0)
    {
        TChanLock lock(m_mutex);
        m_pWr.reset();                  // free up any write buffer
        m_bModified |= m_chanHead.Delete();
    }
    return err;
}

//! Report if this channel is capable of being undeleted
/*!
You _must not_ hold the channel mutex.
\return true if this channel can be undeleted, false if not.
*/
bool CSon64Chan::CanUndelete() const
{
    TChanLock lock(m_mutex);
    return m_chanHead.IsDeleted();
}

//! Attempt to undelete a channel that has previously been deleted.
/*!
You _must not_ hold the channel mutex.
\return S64_OK if the channel was deleted, or a negative error code if the channel is
    not in a deleted state.
*/
int CSon64Chan::Undelete()
{
    TChanLock lock(m_mutex);
    int err = m_chanHead.Undelete();
    m_bModified |= (err == 0);              // if no error, then state changed
    return err;
}

//! Code to fix a channel index or report it does not need fixing
/*!
This should be called before the channel is used or reused and checks that the index
table on disk for the channel is OK. We can survive with a bad index table (it has all
entries set to 0), but it is better to have it set correctly.

You can call this before any channel use when the file is being opened without holding
a mutex, otherwise you MUST hold the channel mutex to call this. Currently this is called
from two places:
- The CSon64Chan constructor when a channel is being reused
- When opening an old file and creating the channels from the disk contents

In both cases, no other thread could have access to the channel, so we do not grab the
channel mutex.
\return -ve error code (usually readonly file), 0 if fixes done, 1 if too few blocks to
        need any fixes, 2 if the channel is OK.
*/
int CSon64Chan::FixIndex()
{
    return m_bmRead.FixIndex();
}

//! We want to reuse an existing channel keeping everything the same
/*!
BEWARE: We currently assume that only one thread will ever call this routine, that
is we are unprotected againt multiple threads. If this becomes an issue we may need
to make this a non-virtual call to do the lock, then call a private virtual to do
the function.

This is called by CSon64File::EmptyFile() and isusually used to quickly abandon the
current sampling state and begin again with the same file and identical settings.

We must make sure that the channel starts to be reused from the beginning. This
means we must forget about buffered data and reset the append list so that we
start from the beginning again.
\return S64_OK if this is done OK, else a negative error code (if the channel
               is unused).
*/
int CSon64Chan::EmptyForReuse()
{
    if (m_chanHead.IsUnused())
        return NO_CHANNEL;

    // We must get the append list written to disk before we clear it (to restart use)
    int iErr = Commit();                    // get the lookup table written
    m_vAppend.clear();                      // force it to reload the list
    m_bmRead.Invalidate();                  // next use must reread
    if (m_pWr)
        m_pWr->clear();                     // forget any buffered data
    m_chanHead.EmptyForReuse();
    m_st.Reset();                           // forget about save/no save times
    m_bModified = true;                     // Header needs writing
    return iErr;
}

//! Prepare a deleted channel for reuse as a possibly different channel type
/*!
BEWARE: We currently assume that only one thread will ever call this routine, that
is we are unprotected againt multiple threads. If this becomes an issue we may need
to make this a non-virtual call to do the lock, then call a private virtual to do
the function.

We are about to reuse a channel. You can only do this if the channel has been
deleted. Once this is done, you cannot undelete the channel. We release all the
channel resources. If there were used blocks we will now be re-using them. You
must hold m_mutChans to do this (so no-one else can mess with channels).
\return S64_OK if done with out a problem, otherwise a CHANNEL_USED if the channel
        is already in use.
*/
int CSon64Chan::ResetForReuse()
{
    if (m_chanHead.IsUsed())	            // Channel must not be in use...
        return CHANNEL_USED;                // ...else this is an error

    // Release any used strings (not done by delete). Strings are zeroed by TChanHead
    m_file.m_ss.Sub(m_chanHead.m_title);
    m_file.m_ss.Sub(m_chanHead.m_units);
    m_file.m_ss.Sub(m_chanHead.m_comment);

    m_pWr.reset(nullptr);                   // delete any owned data block
    m_chanHead.ResetForReuse();             // tell the header (prepares blocks for reuse)
    m_bModified = true;                     // Header needs writing

    return S64_OK;
}

//--------------------------------------------------------------------------------

//! Get the channel type
/*
We do not use a mutex on this as the channel type is set when the channel is created
and cannot be changed while in use. Further, the read is an atomic operation, so
it is hard to see what a TChanLock could do for us except to slow things down.
\return The type of the channel
*/
TDataKind CSon64Chan::ChanKind() const
{
    return m_chanHead.m_chanKind;       // atomic operation, so no lock
}

//! Set the channel title
/*!
\param title The channel title. We do not limit the length, but we suggest maybe no more
             than 30 character (the old system used a maximum of 8).
*/
void CSon64Chan::SetTitle(const string& title)
{
    TChanLock lock(m_mutex);
    s64strid old = m_chanHead.m_title;
    m_chanHead.m_title = m_file.m_ss.Add(title, old);
    if (m_chanHead.m_title != old)
        m_bModified = true;
}

//! Get the channel title
string CSon64Chan::GetTitle() const
{
    TChanLock lock(m_mutex);
    return m_file.m_ss.String(m_chanHead.m_title);
}

//! Set the channel units
/*!
It is up to the application what units it sets. However, a user should bear in mind
that units ure usually things like Volts, mA, mmHg and the like. Applications will tend
to either ignore stupidly long units, or cease to work correctly. We do not impose any
limit on what can be in the string.
*/
void CSon64Chan::SetUnits(const string& units)
{
    TChanLock lock(m_mutex);
    s64strid old = m_chanHead.m_units;
    m_chanHead.m_units = m_file.m_ss.Add(units, old);
    if (m_chanHead.m_units != old)
        m_bModified = true;
}

//! Get the channel units.
/*!
\return A string holding the units.
*/
string CSon64Chan::GetUnits() const
{
    TChanLock lock(m_mutex);
    return m_file.m_ss.String(m_chanHead.m_units);
}

//! Set the channel comment.
/*!
It is up to the application what comment it sets. The 32-bit son system set a limit of
80 characters on the channel comment. You can set longer comments if you wish, but some
applications may truncate them (or be broken).
*/
void CSon64Chan::SetComment(const string& comment)
{
    TChanLock lock(m_mutex);
    s64strid old = m_chanHead.m_comment;
    m_chanHead.m_comment = m_file.m_ss.Add(comment, old);
    if (m_chanHead.m_comment != old)
        m_bModified = true;
}

//! Get the channel comment
string CSon64Chan::GetComment() const
/*!
\return A string holding the channel comment.
*/
{
    return m_file.m_ss.String(m_chanHead.m_comment);
}

//! Associate a physical channel number with the channel
/*!
Physical channel numbers are for information only and can be whatever you like.
By convention we use -1 for channels that were not associated with a physical
device. Some applications may try to extract physical channels numbers when
attempting to reconstruct sampling configurations from a data file.
\param iPhy The physical channel number.
*/
void CSon64Chan::SetPhyChan(int32_t iPhy)
{
    TChanLock lock(m_mutex);
    if (iPhy != m_chanHead.m_iPhyCh)
    {
        m_chanHead.m_iPhyCh = iPhy;
        m_bModified = true;
    }
}

//! Get the physical channel associated with the channel.
/*!
\return The physical channel number that we saved with the channel. This will be -1
        if no physical channel was ever set.
*/
int32_t CSon64Chan::GetPhyChan() const
{
    return m_chanHead.m_iPhyCh;     // Atomic operation
}

//! Set the ideal rate associated with a channel
/*!
This value is typically used by applications to allocate resources and to set y ranges
for rate displays when no other information is available. It is not otherwise used by
the SON64 library.
\param dRate For a waveform-type channel, this is the desired sampling rate in Hz.
             For other channel types, this is the expected mean event rate.
*/
void CSon64Chan::SetIdealRate(double dRate)
{
    TChanLock lock(m_mutex);
    if (dRate != m_chanHead.m_dRate)
    {
        m_chanHead.m_dRate = dRate;
        m_bModified = true;
    }
}

//! Get the ideal rate associated with a channel
/*!
 \return The ideal rate values associated with the channel.
*/
double CSon64Chan::GetIdealRate() const
{
    return m_chanHead.m_dRate;      // atomic operation
}

//! Get the waveform channel divide
/*!
Waveform channels are sampled at a fixed interval in file clock ticks that is stored
in the channel divide. Event channels should return 1.
\return The channel divide for a waveform associated with this channel. If the channel is
        a WaveMark with multiple traces, the rate is the rate for one of the traces.
*/
TSTime64 CSon64Chan::ChanDivide() const 
{
    return m_chanHead.m_tDivide > 0 ? m_chanHead.m_tDivide : 1;    // atomic operation
}

//! Set the number of pre-trigger points for Wavemark type signals
/*!
\param nPreTrig It is up to the caller to ensure that this value is sensible. It is
                not used within the library. It should be in the range 0 to nRows-1.
*/
void CSon64Chan::SetPreTrig(uint16_t nPreTrig)
{
    TChanLock lock(m_mutex);
    if (nPreTrig != m_chanHead.m_nPreTrig)
    {
        m_chanHead.m_nPreTrig = nPreTrig;
        m_bModified = true;
    }
}

//! Get the number of pretrigger points set by SetPreTrig
/*!
Although this was intended for use with an AdcMark channel, it can be used by any
channel type. The value is not used within the library.
\return The current value of the pretrigger points.
*/
int CSon64Chan::GetPreTrig() const
{
    return m_chanHead.m_nPreTrig;       // atomic operation
}

//! Get the number of rows associated with an extended marker type
/*!
This is only defined for extended marker channels, but can be read for any channel type
in case we ever find any uses for it.
\return The number of rows of extended marker data.
*/
size_t CSon64Chan::GetRows() const
{
    return m_chanHead.m_nRows;          // atomic operation
}

//! Get the number of columns associated with an extended marker type
/*!
This is only defined for extended marker channels, but can be read for any channel type
in case we ever find any uses for it.
\return The number of columns of extended marker data.
*/
size_t CSon64Chan::GetCols() const
{
    return m_chanHead.m_nColumns;       // atomic operation
}

//! Set the channel scale value
/*!
This can be used with any channel to set the channel scaling. It is only defined to have
a fixed, library-defined purpose for waveform channels and AdcMark data channel.
\param dScale The new channel scaling. Setting a value of 0.0 is ignored.
*/
void CSon64Chan::SetScale(double dScale)
{
    if (dScale)
    {
        TChanLock lock(m_mutex);
        if (dScale != m_chanHead.m_dScale)
        {
            m_chanHead.m_dScale = dScale;
            m_bModified = true;
        }
    }
}

//! Read back the channel scale value
/*!
This is only defined and used by the library for waveform and AdcMark channels.
\return The channel scale factor.
*/
double CSon64Chan::GetScale() const
{
    return m_chanHead.m_dScale;         // atomic operation
}

//! Set the channel offset value
/*!
This can be used with any channel to set the channel scaling. It is only defined to have
a fixed, library-defined purpose for waveform channels and AdcMark data channel.
\param dOffset The new channel offset.
*/
void CSon64Chan::SetOffset(double dOffset)
{
    TChanLock lock(m_mutex);
    if (dOffset != m_chanHead.m_dOffset)
    {
        m_chanHead.m_dOffset = dOffset;
        m_bModified = true;
    }
}

//! Read back the channel offset value
/*!
This is only defined and used by the library for waveform and AdcMark channels.
\return The channel offset.
*/
double CSon64Chan::GetOffset() const
{
    return m_chanHead.m_dOffset;        // atomic operation
}

//! Read back the size of the items stored in this channel
/*!
This is the number of bytes per item in the channel, so is 2 for Adc, 4 for RealWave,
8 for events, 16 for Markers and 16+n*8 for extended marker data.
\return The size of the items stored in this channel in bytes.
*/
size_t CSon64Chan::GetObjSize() const
{
    return m_chanHead.m_nObjSize;       // atomic operation
}

//! Get the suggested Y range for the channel
/*!
No use is made of these values by the library.
\param dLow     A double to hold the low value
\param dHigh    A double to hold the high value
\return S64_OK  (0) if no error (so always 0).
\sa SetYRange()
*/
int CSon64Chan::GetYRange(double& dLow, double& dHigh) const
{
    TChanLock lock(m_mutex);
    dLow = m_chanHead.m_dYLow;
    dHigh = m_chanHead.m_dYHigh;
    return S64_OK;
}

//! Set the suggested Y range for the channel
/*!
No use is made of these values by the library.
\param dLow     The low value, not the same as dHigh.
\param dHigh    The high value
\return S64_OK  (0) if no error, or BAD_PARAM.
\sa GetYRange()
*/
int CSon64Chan::SetYRange(double dLow, double dHigh)
{
    if (dLow == dHigh)
        return BAD_PARAM;
    TChanLock lock(m_mutex);
    if ((dLow != m_chanHead.m_dYLow) || (dHigh != m_chanHead.m_dYHigh))
    {
        m_chanHead.m_dYLow = dLow;
        m_chanHead.m_dYHigh = dHigh;
        m_bModified = true;
    }
    return S64_OK;
}

//! Get the time of the first item that is held in the write system.
/*!
 When reading, you should only fetch items up to this time, then collect the stuff
 in the write buffer. Beware that if you release the channel lock, the write buffer
 start time may change. You _must_ be holding the channel lock to use this.
 \param tUpto Limits the return time to times before this.
 \return -1 if nothing in the buffer or if the first time in the buffer is >= tUpto,
  else it returns the first time in the write buffer system.
*/
TSTime64 CSon64Chan::WriteBufferStartTime(TSTime64 tUpto) const
{
    if (m_pWr)
    {
        TSTime64 tReturn = m_pWr->FirstTime();
        if (tReturn >= tUpto)
            return tReturn;
    }
    return -1;
}

//! Write Append item to disk if modified
/*!
This also passes any modified information to the read stack. However, I am not convinced
that this can ever be of any use. If we have write buffers, the read system never gets to
see any data that lies in the write buffers.
\param level Index into m_vAppend that needs writing if modified.
\return      0 if OK, or a negative error if the write fails.
*/
int CSon64Chan::SaveAppendIndex(int level)
{
    if (m_vAppend[level].IsModified())
    {
#ifdef DEBUG
        if (!m_vAppend[level].GetTable()->Verify())
            m_vAppend[level].GetTable()->Dump();
#endif
        int errWr = m_file.Write(m_vAppend[level].GetTable(), DLSize, m_vAppend[level].GetDiskOffset());
        if (errWr)              // not sure if any recovery is possible...
            return errWr;       // ...we could allocate a different block?
        m_vAppend[level].ClearModified();

        // The next line has been seen to be effective on a Commit().
        m_bmRead.UpdateIndex(level, m_vAppend[level]); // tell read stack of change
    }
    return 0;
}

//! Add an index item into the index table
/*!
 Given the disk offset at which we are to add the block and the time of the first
 item in the block and the current index level (for recursive use) add the item
 into the index table. If level > 0 this is a recursive call.
 \param doItem   Where on disk the new block is to be added.
 \param time     Time of first item in new block
 \param level    The index into m_vAppend where we want to add this item. This is one less
                 that the level used to identify the block as data (level 0) or index (>0).
 \return         the index into the level at which the item was added or a -ve error code.
*/
int CSon64Chan::AddIndexItem(TDiskOff doItem, TSTime64 time, unsigned int level)
{
    if (level >= m_vAppend.size())          // Create level if not found
    {
        assert(level == m_vAppend.size());  // logic failure if not
        // Create a new top level block, link the channel header to it, point the
        // the first item at the previous top block, then point previous at this.
        TDiskOff doNewTop = m_file.AllocateIndexBlock();    // get new index block
        if (doNewTop == 0)                  // check for failure (disk or file full)
            return NO_BLOCK;

        m_vAppend.push_back(CIndex(m_nChan, 0, 0, level+1)); // new empty top block
        m_vAppend[level].SetDiskOffset(doNewTop);       // set position

        // Point the channel header at the new top block.
        m_chanHead.m_doIndex = doNewTop;    // channel head points at (unwritten) index
        m_bModified = true;                 // we need to update the channel header

        // Point first element of new block at the old top block, point old at the element.
        if (level > 0)                      // link in the previous block list?
        {                                   // Set first item to point at previous top of list
            CIndex& prev = m_vAppend[level-1];      // to save typing
            m_vAppend[level].AddIndexItem(prev.GetDiskOffset(), prev.GetFirstTime()); // set first element
            prev.SetParent(doNewTop, 0, level);     // set parent of previous item
        }

        m_bmRead.Invalidate();              // read block must be refreshed as level is incorrect
    }

    // Our level now exists, so attempt to add to it
    int index = m_vAppend[level].AddIndexItem(doItem, time);  // can we add?
    if (index < DLUItems)                   // index or -ve is a bad error...
        return index;                       // ...either way, we are done

    // If we get here, we have just filled the level, so we need to add an index block at
    // this level.
    TDiskOff doNewIndex = m_file.AllocateIndexBlock();  // a new block
    if (doNewIndex == 0)
        return NO_BLOCK;

    index = AddIndexItem(doNewIndex, time, level+1);
    if (index < 0)
        return index;

    // We will now re-use this entry with the new index block, so save old as must be modified
    // as we have just changed the header link.
    int err = SaveAppendIndex(level);           // Write it if modified
    if (err < 0)
        return err;
    
    m_vAppend[level].clear();   // say block is no longer used (zero m_nItems)
    m_vAppend[level].SetDiskOffset(doNewIndex);	// Set where it will go
    m_vAppend[level].SetParent(m_vAppend[level+1].GetDiskOffset(), index, level+1);
    return m_vAppend[level].AddIndexItem(doItem, time);
}

//! Add a block to the end of the file.
/*!
 The block data is set and the header holds everything except that m_doParent is
 not set. You must hold the channel lock if you call this. You are assumed to have
 already checked for read only!
 If the block has already been written (due to commit) we update what we wrote
 and assume that all the block tracking stuff is already done.
 \param pBlock  Points at the data block to be written. If the disk offset is already
                set we just update it, otherwise we append a new disk block to the channel.
 \return 0 if no error detected or an error code.
*/
int CSon64Chan::AppendBlock(CDataBlock* pBlock)
{
    int err = 0;
    if (pBlock->size() == 0)           // if no data, don't waste our time
    {
        assert(false);                  // this must be a programming error
        return 0;
    }

    TDiskOff doWrite = pBlock->DiskOff();   // already allocated space?
    bool bUpdate = doWrite != 0;            // remember, as must tell read block manager
 
    // If we are reusing channel space, just grab the next block. If update of existing
    // we must preserve the parent block index.
    unsigned int uiParentIndex = bUpdate ? pBlock->GetParentIndex() : 0;
    if (!doWrite && m_chanHead.ReusingBlocks())
    {
        // We are appending. m_vAppend is our chain of blocks to the current last block,
        // and if it is not empty, it is assumed valid. If it is empty, we must load it up.
        // If it is not empty, we move on to the next block except for the special case of
        // no blocks yet used, when we are already pointed at the right place.
        if (m_vAppend.empty() || !m_chanHead.m_nBlocks)	// if no list, or first use
            err = LoadAppendList(true);     // fill in the list for reuse
        else
            err = IncAppendForReuse();		// move to the next
        if (err)
            return err;

        doWrite = GetReuseOffsetSetTime(pBlock->FirstTime());	// next block to use
        if (doWrite)
        {
            m_chanHead.IncReusedBlocks();   // we have used another block, may end reuse
            uiParentIndex = m_vAppend[0].GetReuseIndex();
        }
    }

    // If we still need a block, allocate from the end of the file
    if (!doWrite)                           // if not already allocated disk space
    {
        assert(pBlock->FirstTime() >= m_chanHead.m_lastTime);   // == means a duplicated time
        doWrite = m_file.AllocateDiskBlock();  // set first block position
        if (!doWrite)
            return NO_BLOCK;
        ++m_chanHead.m_nBlocks;				// we have another block

        err = AddIndexItem(doWrite, pBlock->FirstTime(), 0);
        if (err < 0)                        // -ve is error, +ve is the index
            return err;
        else
            uiParentIndex = static_cast<unsigned int>(err);
    }

    // Should we only do these updates if !bUpdate? Surely they will already have
    // these values, in which case getting uiParentIndex above is not needed?
    pBlock->SetDiskOff(doWrite);        // save the offset in the wrapper
    pBlock->SetParent(m_vAppend[0].GetDiskOffset(), uiParentIndex, 0);

//#define DEBUG_WITH_CONTIGUOUS_EVENT_DATA
#ifdef DEBUG_WITH_CONTIGUOUS_EVENT_DATA
    cout << "Append FTime: " << dec << pBlock->FirstTime() << " items: " << pBlock->m_nItems << " do: " << hex << doWrite << '\n';
    if (pBlock->LastTime() - pBlock->FirstTime() != pBlock->m_nItems-1)
    {
        cout << "Discontinuity in times, first at index: ";
        for (int i= 1; i<pBlock->m_nItems; ++i)
        {
            if (pBlock->m_event[i] != pBlock->m_event[i-1]+1)
            {
                cout << i << " time before " << pBlock->m_event[i-1] << " after " << pBlock->m_event[i];
                break;
            }
        }
    }
#endif

    pBlock->m_chanID = m_chanHead.m_chanID; // ensure latest channel ID used
    err = m_file.Write(pBlock->DataBlock(), DBSize, doWrite);
    if (err == 0)
    {
        m_chanHead.m_lastTime = pBlock->LastTime();
        m_file.ExtendMaxTime(m_chanHead.m_lastTime);
        m_bModified = true;                 // Disk version of channel is modified
        pBlock->SetSaved();                 // clear the unsaved flag

        if (bUpdate)                        // if already on disk...
            m_bmRead.UpdateData(*pBlock);   // ...tell the read manager
        m_bmRead.BlockAdded();              // there is a new block
    }
    return err;
}

//! Internal routine used to work out index table depth
/*!
Given a block count, how many lookup table items do we need. If any blocks are
allocated we have 1 item, so for 1..DLUItems the result is 1, for DLUItems+1 to
DLUItems**2 he result is 2, for DLUItems**2+1 to DLUItmes**3 the result is 3. We
should never use this for 0.

For the result of this to be valid, you must either hold the channel mutex when you
call it (so no new blocks get added), or use it at a time when no blocks can be added
such as when opening the channel.
\return The required index table depth
*/
unsigned int CSon64Chan::DepthFor()
{
    uint64_t nBlock = max(m_chanHead.m_nAllocatedBlocks, m_chanHead.m_nBlocks);
    if (nBlock == 0)
        return 0;
    unsigned int n = 1;
    uint64_t nCompare = DLUItems;
    while (nBlock > nCompare)
    {
        nCompare *= DLUItems;
        ++n;
    }
    return n;
}

//! Assign a newly created CDataBlock to m_pWr
/*!
 Call this to set the write buffer in m_pWr (taking ownership of it). If the channel
 already owns disk blocks we read the last block in from disk so we can append to the
 end of the file (for example if we open an existing file, then append data). We must
 cope with a channel being reused, or just extended.

 NBNB: Do not try to be helpful and load the append list anyway in case we are reusing
 blocks. We take care of loading the append list in AppendBlock and soing it here just
 confuses things as LoadAppendList(false) with no blocks causes mayhem!

 \param pBlock   A datablock of the correct type that has just been constructed.
 \return 0 if no problem is detected, otherwise an error code.
*/
int CSon64Chan::InitWriteBlock(CDataBlock* pBlock)
{
    m_pWr.reset(pBlock);                // take ownership of the block
    if (m_chanHead.m_nBlocks == 0)      // if no blocks exist...
        return 0;                       // ...we are done, all is OK
    int err = LoadAppendList(false);    // find where last block lives
    if (err || m_vAppend.empty())       // if problem or nothing to do...
        return err;

    // To get here, the channel has data blocks that are in use and the append list
    // is pointing at the last one. We load the last block as the user may want to
    // append data to it (i.e. restore the situation when last writing to the end).
    TDiskOff offset = m_vAppend[0].GetReuseOffset();
    err = m_file.Read(pBlock->DataBlock(), DBSize, offset);
    pBlock->SetDiskOff( offset );       // tell the block where it lives
    pBlock->NewDataRead();              // clean any cached data
    return err;
}

//! Load m_vAppend with the index blocks to the end of the file
/*!
This is used in two situation, depending on bForReuse. If true, we are reusing disk
blocks so load the append list to point at the current end of the file. Once this is
done, we just keep the list up to date.

If false, we have an existing file and want to append data to the end so we fill in
the list to point at the last block. At the start of a channel this can be called with
no blocks, in which case we can clear the append list and we are done.

\param bForReuse True if we are resuing channel space, false to append
\return 0 if no error detected, else a negative error code.
*/
int CSon64Chan::LoadAppendList(bool bForReuse)
{
    assert(!bForReuse || m_chanHead.ReusingBlocks());
    auto nDeep = DepthFor();        // LU table depth needed for blocks in channel
    m_vAppend.resize(nDeep);		// set table size to match, modified is false
    if (nDeep == 0)                 // if no table size needed (no blocks)...
        return 0;                   // ...we are done

    // First pass. Work out which block indices we want to use. If we are appending
    // then we want to find the position of the last block, not the next one. Append
    // index 0 has the lookups for the data blocks. The final append block is the
    // block pointed at by m_chanHead.m_doIndex. To work out the block indices we
    // work forwards on the first pass:
    auto nWork = bForReuse ? m_chanHead.m_nBlocks : m_chanHead.m_nBlocks-1;
    for (auto it = m_vAppend.begin(); it != m_vAppend.end(); ++it)
    {
        it->SetReuseIndex(nWork % DLUItems);
        nWork /= DLUItems;
    }

    // Second pass. Read the index blocks into the list. We work from the back.
    TDiskOff nextIndex = m_chanHead.m_doIndex;
    uint16_t uParentIndex = 0;              // so we can fix in case index is incorrect
    for (auto it = m_vAppend.rbegin(); it != m_vAppend.rend(); ++it)
    {
        it->SetDiskOffset(nextIndex);
        TDiskLookup* pLU = it->GetTable();  // Tell CIndex item where it lives
        int err = m_file.Read(pLU, DLSize, nextIndex);
        if (err)
            return err;
        pLU->SetParentIndex(uParentIndex);  // patch parent index in case not set
        nextIndex = it->GetReuseOffset();
        uParentIndex = it->GetReuseIndex();
    }
    return 0;
}

//! Move the end of the reused blocks on by 1.
/*!
Given that m_vAppend is set for reuse, move the index on by 1, which means
we are using another block. If this causes any index to overflow, load the
new index. If this uses up all the reuse blocks, set the reusable count to 0.
You must hold the Channel mutex.

\param i The level to move on in the range 0 to m_vAppend.size()-1. External
         code to this function should always use 0.
\return  0 if all OK or a negative error code (disk read error).
*/
int CSon64Chan::IncAppendForReuse(unsigned int i)
{
    int err = 0;
    assert(m_chanHead.ReusingBlocks());
    assert(i < m_vAppend.size());
    if (m_vAppend[i].IncReuseIndex())		// if filled index...
    {
        // We have just filled a block and are about to replace it, so must write.
        err = SaveAppendIndex(i);           // write old block if modified
        if (err == 0)
        {
            err = IncAppendForReuse(i+1);   // do next level up
            if (err == 0)                   // if OK
            {
                TDiskOff offset = m_vAppend[i+1].GetReuseOffset();
                m_vAppend[i].SetDiskOffset(offset);

                TDiskLookup* pLU = m_vAppend[i].GetTable();
                err = m_file.Read(pLU, DLSize, offset);
                pLU->SetParentIndex(m_vAppend[i + 1].GetReuseIndex()); // patch in case not set
            }
        }
    }
    return err;
}

//! Get the disk offset of the next block for reuse and set the times
/*!
When setting the times, if this is the first block of a level, we must iterate
the time setting back up the chain of blocks, and also tell the read system. Call this
either after loading the m_vAppend list, or after calling IncAppendForReuse().
You must hold the Channel mutex.

\param t This is the time of the block that we will place here.
\return  The disk offset of this block, or an error.
*/
TDiskOff CSon64Chan::GetReuseOffsetSetTime(TSTime64 t)
{
    assert(m_chanHead.ReusingBlocks());
    for (unsigned int i=0; i < (unsigned int)m_vAppend.size(); ++i)
    {
        m_vAppend[i].SetReuseTime(t);           // change time of current index item
        m_bmRead.UpdateIndex(i, m_vAppend[i]);  // in case in read tree

        if (m_vAppend[i].GetReuseIndex())       // if not first then we are done as...
            break;                              // ...next level start time already set
    }

    return m_vAppend[0].GetReuseOffset();
}

//! return true if the channel header or write buffer or write indices need writing.
bool CSon64Chan::IsModified() const
{
    TChanLock lock(m_mutex);        // take ownership of the channel
    if (m_bModified)                // if the channel header is modified
        return true;

    if (m_pWr && m_pWr->Unsaved())
        return true;

    if (m_bmRead.Unsaved())
        return true;

    // See if any modified elements of the lookup tree
    for (const auto& item : m_vAppend)
    {
        if (item.IsModified())
            return true;
    }

    return false;
}

//! Commit all buffered and committed data to the operating system
/*!
You _must not_ be holding the channel mutex to call this.
This command makes sure that everything for this channel that is needed on
disk for the file to be viable is written. This means anything in the write buffer
and the write lookup table.
\return S64_OK (0) or an error code.
*/
int CSon64Chan::Commit()
{
    TChanLock lock(m_mutex);            // take ownership of the channel
    int err = 0;

    if (m_pWr && m_pWr->Unsaved())
        err = AppendBlock(m_pWr.get()); // will mark as saved

    // If any read buffer is modified...
    m_bmRead.SaveIfUnsaved();           // save any unsaved modified data

    // Write any modified elements of the lookup tree and mark as not modified
    for (int i = 0; i < (int)m_vAppend.size(); ++i)
    {
        int locErr = SaveAppendIndex(i);
        if (err == 0)                   // We report the first error
            err = locErr;
    }

    if (m_bModified)                    // if the channel header is modified
    {
        int locErr = m_file.WriteChanHeader(m_nChan);
        if (locErr == 0)
            m_bModified = false;
        else if (err == 0)
            err = locErr;
    }

    return err;
}

//! Get a count of the number of bytes used by the channel
/*!
This attempts to calculate how many data bytes would be used by this channel if
the file were closed now.
\return The estimated channel size in bytes.
*/
uint64_t CSon64Chan::GetChanBytes() const
{
    TChanLock lock(m_mutex);            // take ownership of the channel
    uint64_t bytes = m_chanHead.m_nBlocks * DBSize; // used on disk
    if (m_pWr && m_pWr->size() && m_pWr->Unsaved()) // if a write block pending
        bytes += m_pWr->size()*m_chanHead.m_nObjSize;
    return bytes;
}

//! Internal rooutine to get the last time in the write buffer
/*!
You must hold the channel mutex to call this.
\return The time of the last item that in the write buffer. It may not yet have
        been written, but it will be, barring system collapse. The return value
        is -1 if there is no write buffer or it is empty.
*/
TSTime64 CSon64Chan::LastCommittedWriteTime() const
{
    if (m_pWr && !m_pWr->empty())
        return m_pWr->LastTime();
    else
        return -1;
}

//! Internal channel routine to get the max time
/*!
You MUST hold the channel mutex to call this routine.
\return The maximum time held in the channel, or -1 if no data.
*/
TSTime64 CSon64Chan::MaxTimeNoLock() const
{
    TSTime64 tMax = LastCommittedWriteTime();   // see if we have a write buffer
    if (tMax < 0)                               // must look on disk
        tMax = m_chanHead.m_lastTime;           // get last time on disk
    return tMax;
}

//! Get the time of the last committed item on this channel
/*!
You _must not_ hold the channel mutex to call this routine. If there is a write buffer
(that is this is a new file), the maximum time in the channel is the time of the
last item that was written to the channel, regarless of whether we are saving data
or not. If there is circular buffer, and we have a write buffer, then the maximum
time is the last time in the write buffer (even if it has not been written yet).
Otherwise it is the maximum channel time as held in the channel head.
\return The time of the last item in the channel, or -1 if nothing has been written.
*/
TSTime64 CSon64Chan::MaxTime() const
{
    TChanLock lock(m_mutex);            // take ownership of the channel
    return MaxTimeNoLock();
}

//! Internal routine to test a filter
/*!
\param pFilter A reference to a filter pointer that we may set to nullptr if the
               filter would pass everything (so not worth using).
\return        true if this filter would result in no data. Else, false.
*/
bool CSon64Chan::TestNullFilter(const CSFilter*& pFilter)
{
    CSFilter::eActive active = pFilter ? pFilter->Active() : CSFilter::eA_all;
    if (active == CSFilter::eA_none)            // if filtering leaves nothing...
        return true;                            // ...then we are done
    if (active == CSFilter::eA_all)             // if no filtering needed...
        pFilter = nullptr;                      // ...kill the filter
    return false;
}

//! Internal utility to convert short to float using the channel scaling
/*!
\param pf   Output array of floats
\param ps   Input arrar of shorts
\param n    The number of points to convert
*/
void CSon64Chan::short2float(float* pf, const short* ps, size_t n) const
{
    double dScale = m_chanHead.m_dScale/6553.6;
    for (size_t i=0; i < n; ++i)
        *pf++ = static_cast<float>(*ps++ * dScale + m_chanHead.m_dOffset);
}

//! Internal utility to convert float to short using the channel scaling
/*!
\param ps   Output arrar of shorts
\param pf   Input array of floats
\param n    The number of points to convert
*/
void CSon64Chan::float2short(short* ps, const float* pf, size_t n) const
{
    double dScale =  6553.6 / m_chanHead.m_dScale;
    short sUpper = SHRT_MAX;
    short sLower = SHRT_MIN;
    double dUpper = SHRT_MAX / dScale + m_chanHead.m_dOffset;
    double dLower = SHRT_MIN / dScale + m_chanHead.m_dOffset;
    if (dUpper < dLower)
    {
        std::swap(dUpper, dLower);
        std::swap(sUpper, sLower);
    }

    for (size_t i=0; i<n; ++i)
    {
        double f = *pf++;                    // get next value
        if (f >= dUpper)
            *ps = sUpper;
        else if (f <= dLower)
            *ps = sLower;
        else
        {
            f = (f - m_chanHead.m_dOffset) * dScale;    // convert to integer
            *ps = static_cast<short>((f >= 0) ? f+0.5 : f-0.5);
        }
        ++ps;
    }
}

/*
Get wavedata from Adc channel as floats. To avoid allocating a buffer twice, cast the
second half of the buffer to a short* read data, then convert to float.
pData   Array of float used as workspace, then to return the data.
r       A data range to collect.
tFirst  The time of the first point returned (if we got any data)
pFilter May be nullptr. A filter to used on the data.
return  The number of points read into the buffer.
*/
int CSon64Chan::ReadData(float* pData, CSRange& r, TSTime64& tFirst, const CSFilter* pFilter)
{
    short* pS = (short*)(pData + r.Max()/2);        // a heinous cast to reuse second half
    int iRet = ReadData(pS, r, tFirst, pFilter);    // collect the data
    if (iRet > 0)
        short2float(pData, pS, static_cast<size_t>(iRet));
    return iRet;
}

//! Generic routine to find the previous time on a channel.
/*!
We Look in any write buffer first, then look in the channel data. This returns the earliest
start time to read from to get up to r.Max() data points. If there are no data points before
t.Upto(), the result is -1. For waveform channels, the potentially read data must be
contiguous.
\param r       The range to search. Upto() is the point after the range, not to be included.
               From() is the earliest to include, Max() is the maximum points to skip back
               First() is true if no points yet found (needed for waves).
\param pFilter Either nullptr or points at a filter to apply.
\param bAsWave If true and the data could be treated as a wavefrom or as events, treat as
               a waveform.
\return        Time of the start of the read or -1 if no data at all before Upto().
*/
TSTime64 CSon64Chan::PrevNTime(CSRange& r, const CSFilter* pFilter, bool bAsWave)
{
    if (!r.HasRange())                          // Something is possible
        return -1;

    TChanLock lock(m_mutex);                    // take ownership of the channel

    // bAsWave means that this is an extended marker that is to be treated as a wave.
    // However, lots of code will set this true for waveforms in error, so we protect
    // against this by cancelling this for waveforms.
    if (bAsWave &&
        ((ChanKind() == Adc) || (ChanKind() == RealWave)))
        bAsWave = false;

    // Start by looking in the write buffer. If this starts before the desired time we
    // will have some data in here.
    if (m_pWr)
    {
        TSTime64 t = bAsWave ? m_pWr->PrevNTimeW(r, pFilter, m_chanHead.m_nRows, m_chanHead.m_tDivide)
                             : m_pWr->PrevNTime(r, pFilter);   // ask the write buffer
        if (!r.HasRange())                      // if we found the item, or...
            return t;                           // ...time limit says done
    }

    // Subtle point. We seek r.Upto()-1 because if the disk buffer started at Upto(),
    // asking for this would fail as the block would be off the end of the range.
    int err = m_bmRead.LoadBlock(r.Upto()-1);   // get the first block to consider

    // If err == 0, the current block holds data at or after the Upto() time. If err == 1, we are
    // off the end of the file. If we have a block, it is the one before the one we want, so we
    // can continue.
    if ((err == 1) && !m_bmRead.Valid())        // If Upto() is past the end and no read block...
        return -1;                              // ...there is no previous point

    // If err < 0, we have an error. If err == 0 we have a block holding data at or after the time
    // we wanted. If err == 1, the block holds data before the time we searched for. Either way we
    // should start skipping backwards.
    if (err >= 0)
    {
        do
        {
            TSTime64 t = bAsWave ? m_bmRead.DataBlock().PrevNTimeW(r, pFilter, m_chanHead.m_nRows, m_chanHead.m_tDivide)
                                 : m_bmRead.DataBlock().PrevNTime(r, pFilter);
            if (!r.HasRange())                      // Found, or not possible...
                return t;                           // ...we are done
            if (r.TimeOut())                        // see if we have read enough blocks...
                return CALL_AGAIN;                  // ...lets run round again, please
            err = m_bmRead.PrevBlock();             // go back one block, err==1 hit the start
        }while (err == 0);
    }

    return (err < 0) ? err : -1;                // either an error, or fell off the end
}

//! Generic routine to read event times from the channel (unless overridden)
/*!
Read data from the channel. If the write buffer exists and overlaps the read request,
use the data in the write buffer in preference to the data on disk.
\param pData    Pointer to space to read the data of size at least r.Max().
\param r        The data time range and points to read.
\param pFilter  Either nullptr or a filter to applyto the data
\return         The number of data points or a negative error code
*/
int CSon64Chan::ReadData(TSTime64* pData, CSRange& r, const CSFilter* pFilter)
{
    if (!r.HasRange() || TestNullFilter(pFilter)) // if nothing possible...
        return 0;                           // ...bail out now

    int nRead = 0;                          // will be the count of read data

    TChanLock lock(m_mutex);                // take ownership of the channel

    // If we have a write buffer, and our time range includes data in the buffer, use that
    // in preference to reading from the disk. If the write buffer exists, it holds the
    // last block and holds at least 1 data value.
    TSTime64 tBufStart = m_pWr ? m_pWr->FirstTime() : TSTIME64_MAX;

    if (r.From() < tBufStart)   // Only look on disk if worth doing
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

//! Generic routine to read marker data from the channel (unless overridden).
/*!
If the write buffer exists and overlaps the read request, use the data in the write
buffer in preference to the data on disk.
\param pData    Pointer to space to read into or at least r.Max() markers
\param r        The range of data to read.
\param pFilter  Either nullptr or a filter for the data.
\return         The number of items read or a negative error code.
*/
int CSon64Chan::ReadData(TMarker* pData, CSRange& r, const CSFilter* pFilter)
{
    if (!r.HasRange() || TestNullFilter(pFilter)) // if nothing possible...
        return 0;                           // ...bail out now

    int nRead = 0;                          // will be the count of read data

    TChanLock lock(m_mutex);                // take ownership of the channel

    // If we have a write buffer, and our time range includes data in the buffer, use that
    // in preference to reading from the disk. If the write buffer exists, it holds the
    // last block and holds at least 1 data value.
    TSTime64 tBufStart = m_pWr ? m_pWr->FirstTime() : TSTIME64_MAX;

    if (r.From() < tBufStart)   // Only look on disk if may hold data
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

//-------------------------- EventBoth support -----------------------------------

//! Set the initial level for a level channel when writing
/*!
 This is important when a level channel is empty as at that point we have no way to know what the
 initial level is. This should be called before any data is written to the channel.
 \param bInitLevel  Set true if initially high, false for initially low
 \return            S64_OK (0) or a negative error code
*/
int CSon64Chan::SetInitLevel(bool bInitLevel)
{
    TChanLock lock(m_mutex);            // take ownership of the channel

    if (ChanKind() != EventBoth)        // ChanKind() does not grab the mutex
        return CHANNEL_TYPE;

    // If we accumulate lots of flags we may want to have specific routines for setting and
    // clearing flags in a generic manner.
    if (GetInitLevel() != bInitLevel)   // are we attempting a change
    {
        if (bInitLevel)
            m_chanHead.m_flags |= ChanFlag_LevelHigh;
        else
            m_chanHead.m_flags &= ~ChanFlag_LevelHigh;
        m_bModified = true;             // Header needs writing
    }
    return S64_OK;
}

//! Get initial level of level channel
/*!
 The initial channel level must be set before any data can be written, so this is assumed to be
 unchanging once the channel is created. This does not grab the channel mutex, so you should
 already hold it before you call this, or you must be sure that this is not an issue.
*/
bool CSon64Chan::GetInitLevel() const
{
    return (m_chanHead.m_flags & ChanFlag_LevelHigh) != 0;
}
