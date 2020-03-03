// s64blkmgr.cpp
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

//! \file s64blkmgr.cpp
//! \brief Code to manage a data block and the list of indices that locate it on disk.

#include <assert.h>
#include "s64priv.h"
#include "s64chan.h"
#include <iostream>

using namespace ceds64;
using namespace std;

//-------------- CBlockManager ----------------------------------------------
//! Construct a data block for a channel
/*!
\param rChan    The CSon64Chan that owns this block. We take a reference to it.
*/
CBlockManager::CBlockManager(CSon64Chan& rChan)
    : m_chan( rChan )
    , m_nBlock( -1 )
{
    // Warning: rChan must not be used in constructor as may be partially constructed
}

// Utility to read a disk lookup block into m_vIndex[i].
int CBlockManager::ReadIndex(CIndex& index, TDiskOff pos)
{
    assert(pos != 0);
    int err = m_chan.m_file.Read(index.GetTable(), DLSize, pos);
    if (err == 0)
    {
#ifdef _DEBUG
        if (!index.GetTable()->Verify())
            index.GetTable()->Dump();
#endif
        assert((index.GetTable()->m_nItems <= DLUItems) && (index.GetTable()->m_nItems > 0));
        index.SetDiskOffset(pos);  // item is now up to date
    }
    return err;
}

//! Called by writing code when moving an index block onto the next.
/*!
This is to warn us that a block (which we might be holding) has changed. If so, we
copy the new state. An alternative is to set m_nBlock negative.
\param level    The index into m_vIndex that this block corresponds to.
\param index    The index block from the writing code
*/
void CBlockManager::UpdateIndex(unsigned int level, const CIndex& index)
{
    if (m_nBlock < 0)                   // if reload is forced, nothing to do
        return;
    if (level >= m_vIndex.size())       // If the index size has changed...
        m_nBlock = -1;                  // ...force a reload
    else if (m_vIndex[level].GetDiskOffset() == index.GetDiskOffset())
        m_vIndex[level] = index;        // it matches our block, so copy it
}

//! Update this block if it matches a block that has just been written to disk
/*!
Called by writing code when it writes to disk a change to an existing data block.
This might render the version held in the datablock invalid. This can happen after
a commit, though operations should look in the write buffer first in any case. An
alternative implementation would be to set the disk offset to an impossible value
(0) to force it to reload.
\param block    The block that has just been written.
*/
void CBlockManager::UpdateData(const CDataBlock& block)
{
    if (m_pDB && (m_pDB->DiskOff() == block.DiskOff())) // if we are holding this block...
    {
        *(m_pDB->DataBlock()) = *(block.DataBlock());   // ...update as if read from disk
        m_pDB->NewDataRead();               // forget any cached pointers
    }
}

//! Read into our datablock unless we already have the data.
/*!
\param pos  The position on disk to read from, which must be properly aligned.
\return     0 if OK, else an error code. Now, we may not need to do this if we
            already hold the block. If the block is in the channel write buffer, we
            will not be here in any case as the write buffer will catch this data.
*/
int CBlockManager::ReadDataBlock(TDiskOff pos)
{
    assert(m_pDB &&                         // Trap stupid errors
           (m_vIndex[0].GetLevel() == 1) && // Make sure table seems OK
           ((pos & (DBSize-1)) == 0));      // Not completely bad read - release test?
    if (pos == m_pDB->DiskOff())            // if we already have it...
        return 0;                           // ...we are done, no read needed

    SaveIfUnsaved();                        // if the block is dirty save it
    TDataBlock* pb = m_pDB->DataBlock();
    int err = m_chan.m_file.Read(pb, DBSize, pos);
    m_pDB->SetDiskOff(pos);
    m_pDB->NewDataRead();                   // tell container to clean cached old data
    m_pDB->SetSaved();
    return err;
}

//! Calculate the number of items in the last index block at each level
/*!
If we are reusing this channel, the number of items held by the index tables relate to
the number of allocated blocks, not the number of used blocks. This calculates the number
of valid items that exist in the last index block at each level.
*/
void CBlockManager::CalcReuse(size_t nLevel)
{
    m_vReuse.resize(nLevel);                // clears if not reusing
    if (nLevel)
    {
        uint64_t nLeft = m_chan.m_chanHead.m_nBlocks-1; // highest valid index
        for (auto& n : m_vReuse)
        {
            n = (nLeft % DLUItems) + 1;     // items in last block at this level
            nLeft /= DLUItems;
        }
    }
}

//! Called by the write code when a new block has been added.
/*!
The channel blocks have increased by 1. If we have a reuse vector, and we have a block, we
should increment the counts of items at each level. In all other cases we let the LoadBlock
routine sort it all out.
*/
void CBlockManager::BlockAdded()
{
    if ((m_nBlock >= 0) && !m_vReuse.empty())
    {
        for (auto& n : m_vReuse)
        {
            if (n >= DLUItems)  // If this fills the block...
                n = 1;          // ...we have started a new block
            else
            {
                ++n;            // Not filled, so increment last n... 
                break;          // ...and we are done
            }
        }
    }
}

//! Load the block manager with a block that includes a nominated time.
/*!
You MUST hold the channel mutex to perform this operation.

m_vIndex holds one or more index blocks. Index 0 holds a list of disk offsets of data
blocks. Higher indexes hold lists of disk offsets to index blocks. The final index is
the index block that the channel header points at. Once a data block is written it is
fixed for that channel, so disk index blocks get data added until they are full, but
disk index items are not modified. However, if data is written, the indexes may expand,
in which case m_nBlock will be set negative to force a reload.

We must also allow for reuse of a channel. In this case the disk index blocks will refer
to blocks that have not yet been used, so must not be used for searches for data.

\param tFind The time to search for. Get the first block that holds it or a later time.
\return    0 if the block is found, 1 if no block holds data, -ve for error.
*/
int CBlockManager::LoadBlock(TSTime64 tFind)
{
    if (m_chan.m_chanHead.m_nBlocks == 0)  // can do nothing if nothing is written
        return 1;                   // no block holds any data

    // bReuse will be true if we are reusing previously allocated blocks
    bool bReuse = m_chan.m_chanHead.ReusingBlocks(); 

    int err = 0;

    // m_nBlock is set negative when we have no block read. It is also set negative when
    // a writing operation has increased the number of data levels.
    if (m_nBlock < 0)               // must read the entire chain in block by block
    {
        size_t n = m_chan.DepthFor();
        m_vIndex.clear();           // remove memory of previous data (fixes reusing bug)
        m_vIndex.resize(n);         // make sure we have the correct size
        CalcReuse(bReuse ? n : 0);  // recalculate reuse indices (if needed)
    }

    // Either not reusing a channel, OR the resuse list MUST match the index list.
    // We could reuse the m_indexReuse member of CIndex and eliminate m_vReuse.
    assert(!bReuse || (m_vReuse.size() == m_vIndex.size()));

    const VIndex& wrIndex = m_chan.m_vAppend;   // local ref to save typing
    bool bHasWrite = wrIndex.size() == m_vIndex.size();

    // Values used when bReuse is true to work out actual block to use
    int iReuseIndex = (int)m_vIndex.size();     // if reusing, m_vReuse index +1

    // Fill in the lookup table for all blocks that are not already read.
    TDiskOff doLast = m_chan.m_chanHead.m_doIndex; // first index block to read
    uint64_t nBlock = 0;            // to build the block number
    unsigned int ub = 0;            // Parent index of index blocks
    auto wit = wrIndex.rbegin();    // iterator to the reverse list of write blocks
    for (auto it = m_vIndex.rbegin(); it != m_vIndex.rend(); ++it)
    {
        // If the tree matches the write tree, we can copy data from the write tree.
        if (bHasWrite && (wit->GetDiskOffset() == doLast))
        {
            // If the block header has changed, then we must copy it again. This should work
            // as any modification made by writes should increase the number of items (at the least)
            if (static_cast<TDiskBlockHead>(*it->GetTable()) != static_cast<const TDiskBlockHead>(*wit->GetTable()))
                *it = *wit;                 // copy the write index
#ifdef _DEBUG
            if (!it->GetTable()->Verify())
                it->GetTable()->Dump();
#endif
            ++wit;                          // on to the next block in the index tree
        }
        else if ((m_nBlock < 0) ||          // If loading the full list or the disk offset...
                 (it->GetDiskOffset() != doLast))   // ...doesn't match
        {
            err = ReadIndex(*it, doLast);   // update the index entry
            it->SetParentIndex(ub);         // in case not set due to bug in old versions
            bHasWrite = false;              // Tree must match all the way, so no longer using it.
        }

        // If reusing blocks and still at the end (if we are not at the end, then all the
        // blocks are full, so no need to reduce the counts), limit the indices used.
        size_t nBlockSize = bReuse ? m_vReuse[--iReuseIndex] : 0;
        ub = it->UpperBound(tFind, nBlockSize); // find first entry past
        if (ub)                             // leave 0 alone...
            --ub;                           // ...else decrement
        if (bReuse && (ub < nBlockSize-1))  // if not the last block, then...
            bReuse = false;                 // ...no need to mess with indices

        doLast = it->GetTable()->m_items[ub].m_do;
        assert(doLast != 0);
        nBlock = (nBlock * DLUItems) + ub;  // the block number
    }

    if (err == 0)
    {
        err = ReadDataBlock(doLast);
        if (err == 0)
        {
            m_nBlock = nBlock;              // this block is OK...

            // If block does not have wanted data, we want the next block
            if (m_pDB->LastTime() < tFind)  // we want the next block
                err = NextBlock();          // so move on, if we can
        }
    }

#ifdef _DEBUG
    if ((err == 0) && (m_pDB->LastTime() < tFind))
        cout << "LoadBlock( " << tFind << " ), LastTime: " << m_pDB->LastTime() << '\n';
#endif
    if (err < 0)                            // if anything went wrong...
         m_nBlock = -1;                     // ...our read tree is bad
    return err;
}

//! Given that m_vRead and m_pRd hold a block, move on to the next block.
/*!
This cannot be used if m_nBlock is < 0. You MUST hold the channel mutex. Note that
This works for the case where we are reusing blocks as reused blocks are in the same
sequence and we check for being off the end of the used blocks.
\param i This is the level and is ONLY used internally to recurse through the block
         index list. It is always 0 (the layer pointing at data blocks) when called
         non-recursively.
\return  0 if next block was read, 1 if no next block (no change to read structure),
         -ve if there was an error, read structure no longer valid.
*/
int CBlockManager::NextBlock(unsigned int i)
{
    assert(m_nBlock >= 0);                  // if this fires, another thread has written
    size_t n;                               // the index to increment
    if (i == 0)
    {
        assert(m_pDB && !m_vIndex.empty());  // madness check
        // if we are at the end...
        if (static_cast<uint64_t>(m_nBlock+1) >= m_chan.m_chanHead.m_nBlocks)
            return 1;                        // ...then we are done
        TDataBlock* pb = m_pDB->DataBlock(); // saves typing
        assert(pb->GetParentBlock() == m_vIndex[0].GetDiskOffset());
        n = pb->GetParentIndex();           // index to increment
    }
    else
    {
        if (i >= m_vIndex.size())           // we are off the top...
        {
            assert(false);                  // should NOT happen
            return 1;                       // ...so no next block
        }
        n = m_vIndex[i-1].GetParentIndex(); // index to increment
    }

    // We must be careful when reading indices as there may be a write buffer associated
    // with the channel. If there is, we must use the write buffer version of any index.
    bool bHasWr = m_chan.m_vAppend.size() == m_vIndex.size();
    int err = 0;
    TDiskLookup* pLU = m_vIndex[i].GetTable();
    if (pLU->m_nItems <= n + 1)             // if this will pass the end...
    {
        err = NextBlock(i+1);               // ...get the next level to increment
        if (err == 0)
        {
			unsigned int n1 = m_vIndex[i].GetParentIndex() + 1;	// new parent index
			if (n1 >= DLUItems)				// if we have reached block end...
				n1 = 0;						// ...we will be at block start
            TDiskLookup* pLU1 = m_vIndex[i+1].GetTable();   // next layer up table
            assert(pLU1->m_nItems);                         // madness check
            TDiskOff doRead = pLU1->m_items[n1].m_do;       // the block to read
            if (bHasWr && (m_chan.m_vAppend[i].GetDiskOffset() == doRead))
                m_vIndex[i] = m_chan.m_vAppend[i];
            else
            {
                err = ReadIndex(m_vIndex[i], doRead);
                m_vIndex[i].SetParentIndex(n1); // in case it is not set
            }
            assert(n1 == m_vIndex[i].GetParentIndex());
        }
        n = 0;                              // start at first item
    }
    else
        ++n;                                // the new index

    // If this is the bottom level of the tree, we must read the data block.
    if ((i == 0) && (err == 0))
    {
        err = ReadDataBlock(pLU->m_items[n].m_do);
        if (err == 0)
            ++m_nBlock;
        else
            m_nBlock = -1;
    }
    return err;
}

//! Given that m_vRead and m_pRd hold a block, move to the previous block.
/*!
This can not be used if m_nBlock is < 0. You MUST hold the channel mutex. Note that
This works for the case where we are reusing blocks as reused blocks are in the same
sequence and we check for being off the end of the used blocks.
\param i This is the level and is ONLY used internally to recurse through the block
         index list. It is always 0 (the layer pointing at data blocks) when called
         non-recursively.
\return  0 if next block was read, 1 if no next block (no change to read structure),
         -ve if there was an error, read structure no longer valid.
*/
int CBlockManager::PrevBlock(unsigned int i)
{
    assert(m_nBlock >= 0);                  // if this fires, another thread has written
    size_t n;                               // the index to decrement
    if (i == 0)
    {
        assert(m_pDB && !m_vIndex.empty()); // madness check
        if (m_nBlock == 0)                  // if we are at the start...
            return 1;                       // ...then we are done
        TDataBlock* pb = m_pDB->DataBlock(); // saves typing
        assert(pb->GetParentBlock() == m_vIndex[0].GetDiskOffset());
        n = pb->GetParentIndex();           // index to decrement
    }
    else
    {
        if (i >= m_vIndex.size())           // we are off the top...
        {
            assert(false);                  // should NOT happen
            return 1;                       // ...so no next block
        }
        n = m_vIndex[i-1].GetParentIndex(); // index to decrement
    }

    // We must be careful when reading indices as ther may be a write buffer associated
    // with the channel. If there is, we must use the write buffer version of any index.
    // bool bHasWr = m_chan.m_vAppend.size() == m_vIndex.size();
    int err = 0;
    TDiskLookup* pLU = m_vIndex[i].GetTable();
    if (n == 0)                             // if this will pass the end...
    {
        err = PrevBlock(i+1);               // ...get the next level to decrement
        if (err == 0)
        {
  			unsigned int n1 = m_vIndex[i].GetParentIndex();	// old parent index
			n1 = n1 ? n1 - 1 : DLUItems-1;	                // previous index, wrapping around if needed
            TDiskLookup* pLU1 = m_vIndex[i+1].GetTable();   // next layer up table
            TDiskOff doRead = pLU1->m_items[n1].m_do;		// the block to read

            // Surely... the previous block, by definition, cannot be the last one, so cannot be
            // in the disk write buffer! Clean up later if this causes no problems (but comment next line)
//            if (bHasWr && (m_chan.m_vAppend[i].GetDiskOffset() == doRead))
//                m_vIndex[i] = m_chan.m_vAppend[i];
//            else
                err = ReadIndex(m_vIndex[i], doRead);
                m_vIndex[i].SetParentIndex(n1);

            assert(pLU->m_nItems == DLUItems);              // madness check, previous must be full
        }
        n = DLUItems-1;                     // start at last item
    }
    else
        --n;                                // the new index

    // If this is the bottom level of the tree, we must read the data block (unless already read)
    if ((i == 0) && (err == 0))
    {
        err = ReadDataBlock(pLU->m_items[n].m_do);
        if (err == 0)
            --m_nBlock;
        else
            m_nBlock = -1;
    }
    return err;
}

//! Save this data block if it exists, has a known disk address and is modified
/*!
If this block is modified, write it to disk (as long as it exists etc). This is only
to be used after changing data. NB: This does not change the m_chanID of the block as
this is only done when reusing in the AppendBlock() code where writes are normally done.
Any data written here has already been written, so will have the correct m_chanID.
*/
int CBlockManager::SaveIfUnsaved()
{
    assert(m_pDB);
    TDiskOff pos = m_pDB->DiskOff();
    if (!m_pDB->Unsaved() || (pos == 0))    // if saved, or no position...
        return 0;                           // ...we are done
    assert(m_vIndex[0].GetLevel() == 1);    // just to be sure we are not insane
    TDataBlock* pb = m_pDB->DataBlock();
    int err = m_chan.m_file.Write(pb, DBSize, pos);
    m_pDB->SetSaved();
    return err;
}

//! Check that the indices for the block are OK and fix them if they are not
/*!
There was a fault where the parent index part of the disk address was not set in early
builds of the library. This will attempt to fix the problem if it exists. This only
affected the index blocks, the index was written correctly to the data blocks.

This should be called before anything else is done with the channel. That is, there is
no write buffer, so all we need do if fix, if needed, the index blocks.

\return 0 if errors were fixed, 1 if too small to have errors, 2 if the file has
        correctly written indexes (so no need for further checking).
*/
int CBlockManager::FixIndex()
{
    m_nBlock = -1;                  // force re-read after we mess about
    unsigned int n = m_chan.DepthFor();
    if (n < 2)                      // If not two levels there can be no problem
        return 1;                   // too small to have a problem

    if (m_vIndex.size() != n)
        m_vIndex.resize(n);         // make sure we have the correct size

    // Fill in the lookup table so that we have the index for block DLUItems (0-based)
    // loaded. There is no point starting at 0, as we cannot tell if this is bad or not.
    TDiskOff doLast = m_chan.m_chanHead.m_doIndex; // first index block to read
    for (int i = n - 1; i >= 0; --i)            // fill from the end to match other use
    {
        int err = ReadIndex(m_vIndex[i], doLast);   // update the index entry
        if (err)
            return err;
        int iTabIndex = i > 1 ? 0 : 1;          // so bottom level will show the problem
        doLast = m_vIndex[i].GetTable()->m_items[iTabIndex].m_do; // next level
    }

    // m_vIndex[0] holds the table for blocks DLItems..2*DLItems-1 and should have a
    // parent index of 1. If it does, then there is nothing to be fixed and we are done.
    if (m_vIndex[0].GetParentIndex() == 1)
        return 2;                               // flag nothing to be done

    return PatchIndex(n - 1, 0);                // Fix all the indices
}

//! Patch the parent index of the index block at a level and all its child blocks
/*!
This is used recursively, starting at the top of the tree (with the highest level). The
index block for the nominated level is already loaded. Only write if a change. You must
hold the channel mutex.
\param level    The index of the index block in m_vIndex
\param uiParent The index of this block in the parent index
\return         0 if all OK or a negative error code or 1 if a patch made
*/
int CBlockManager::PatchIndex(unsigned int level, unsigned int uiParent)
{
    int err = 0;
    CIndex& ind = m_vIndex[level];              // local reference to save typing
    if (ind.SetParentIndex( uiParent ))         // If an index mismatch...
        err = m_chan.m_file.Write(ind.GetTable(), DLSize, ind.GetDiskOffset());

    if (!err && (level > 0))                    // recursively work through lower levels
    {
        CIndex& lower = m_vIndex[level - 1];    // The next lower index
        TDiskLookup& dlu = *ind.GetTable();     // The lookup table to iterate through

        // If reusing we must scan until we see a zero in the table as m_nItems relates to the
        // items in use, not the total we have.
        const unsigned int nTest = m_chan.m_chanHead.ReusingBlocks() ? DLUItems+1 : dlu.m_nItems;
        for (unsigned int i = 0; (i < nTest) && dlu.m_items[i].m_do && !err; ++i)
        {
            int err = ReadIndex(lower, dlu.m_items[i].m_do);
            if (err == 0)
                err = PatchIndex(level - 1, i);
        }
    }
    return err;
}