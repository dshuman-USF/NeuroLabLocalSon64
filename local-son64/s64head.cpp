// s64head.cpp
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

#include "s64priv.h"
#include <assert.h>
using namespace ceds64;
using namespace std;
//! \file s64head.cpp
//! \brief Implementation of TFileHead, TFileHeadID and TDiskBlockHead and TSon64File header stuff
//! \internal


//! Structure used to define single data block when we extend the header.
//! \internal
struct TExpBlock : public TDiskBlockHead
{
    enum {nFill = (DBSize-DBHSize) / sizeof(uint64_t)}; //!< Extra uint64_t items needed to fill
	uint64_t space[nFill];                              //!< Variable to make up the space
	TExpBlock(){fill_n(space, size_t(nFill), 0);}       //!< Constructor zeros the space
};

TFileHeadID::TFileHeadID(uint8_t major, uint8_t minor)
    : m_Minor( minor )
    , m_Major( major )
{
    copy_n("S64", 3, m_Ident.begin());   // set first 3 char values from the literal string
    m_Ident[0] = 'S';
    m_Ident[1] = '6';
    m_Ident[2] = '4';
    m_Ident[3] = 'a' + DBSizeSh - 1;
    m_Ident[4] = 'a' + DLSizeSh - 1;
    m_Ident[5] = 0;
}

//! Code to test if this file header is acceptable or not
/*!
 Call this with no arguments to check if the header matches this implementation of the file
 system, that is it was built with the same DBSizeSh and DLSizeSH as this code has. Otherwise
 you can check that it is likely to be a Son64 file and return the data block sizes.
 \param pBlkSz  If nullptr, m_Ident[3] must match the size set by DBSizeSh. If not
                nullptr, we return the implied block size, as long as it is legal.
 \param pLUpSz  If nullptr, m_Ident[4] must match the size set by DLSizeSh. If not
                we return the implied lookup table size.
 \return        true if no return sizes and the header matches what we expect. If we have any
                returned sizes, true if the returned sizes are possible. Otherwise, false.
*/
bool TFileHeadID::IdentOK(int32_t* pBlkSz, int32_t* pLUpSz) const
{
    bool bOK = (m_Ident[0] == 'S') && (m_Ident[1] == '6') && (m_Ident[2] == '4') && !m_Ident[5];
    if (bOK)    // if the S64 and the final 0 are OK, look at the sizes
    {
        int iBlkSh = m_Ident[3] - 'a' + 1;      // The DBSizeSh value the file was built with
        int iLUpSh = m_Ident[4] - 'a' + 1;      // The DLSizeSh value the file was built with
        if (pBlkSz)
        {
            if ((iBlkSh < DBSizeShLo) || (iBlkSh > DBSizeShHi))
                bOK = false;
            else
                *pBlkSz = (1 << iBlkSh);
        }
        else
            bOK = iBlkSh == DBSizeSh;

        if (pLUpSz)
        {
            if ((iLUpSh < DBSizeShLo) || (iLUpSh > DBSizeShHi))
                bOK = false;
            else
                *pLUpSz = (1 << iLUpSh);
        }
        else
            bOK = bOK && (iLUpSh == DLSizeSh);
    }
    return bOK;
}

// This is a nasty hack to convert the types.
TFileHeadID::TFileHeadID(TDiskOff offset)
{
    const TFileHeadID* pFHID = reinterpret_cast<const TFileHeadID*>(&offset);
    *this = *pFHID;
}

//------------------ TDiskBlockHead stuff -----------------------

//! Set the parent information for this block
/*!
\param doParent The disk offset of the parent block. This must be a multiple of DLSize or
                something is badly wrong and the file system will fail.
\param index    The index in the parent block that points at this block. 0..IndexMask
\param level    The level of this block. 0=data, 1 up for index.
*/
void TDiskBlockHead::SetParent(TDiskOff doParent, unsigned int index, unsigned int level)
{
    assert((doParent & (DLSize-1)) == 0);   // could be data or lookup
    assert(index <= IndexMask);
    assert(level <= LevelMask);
    m_doParent = doParent | ((level << LevelSh) | index);
}

//! Utility added to help fix a bad index to fix a bug where the index was not set
/*!
\param index The index of this disk block in the parent index block. If this differs
             from the currenly held index, change the index.
\return      true if the proposed index was different, else false if no change.
*/
bool TDiskBlockHead::SetParentIndex(unsigned int index)
{
    assert(index <= IndexMask);
    if (GetParentIndex() != index)
    {
        m_doParent &= static_cast<int64_t>(-1) << LevelSh;  // clear index bits
        m_doParent |= (index & IndexMask);
        return true;
    }
    return false;
}

//------------------TFileHead stuff ---------------------------

//! Initialise the file header block
/*!
\param nChannels    The number of channels in the file. This is expected to be more
                    than the minimum number MINCHANS and less than or equal to MAXCHANS.
\param nFUser       The user data size on disk.
*/
void TFileHead::Init(uint16_t nChannels, uint32_t nFUser)
{
    TDiskBlockHead::Init(TFileHeadID(REV_MAJOR, REV_MINOR), 0xffff);
    m_creator.clear();
    m_tdZeroTick.clear();
    m_dSecPerTick = 1e-6;       // 1 us per tick by default
    m_maxFTime = -1;            // no data yet in the file

    fill(m_comments.begin(), m_comments.end(), 0); // set all comments to empty
    fill(m_HeaderExt.begin(), m_HeaderExt.end(), 0);    // set no header extensions used
    m_nHeaderExt = 0;

    m_nChannels = nChannels;    // channels in the file
    m_nChanHeadSize = sizeof(TChanHead);

    m_nUserSize = nFUser;       // size of the user area
	m_nUserStart = sizeof(TFileHead); // it follows the file header
	m_nChanStart = m_nUserStart + m_nUserSize;
	m_nStringStart = m_nChanStart + m_nChannels*m_nChanHeadSize;

    m_doNextBlock = DBSize;     // The header is always assigned (0 means size exceeded)
    m_doNextIndex = 0;          // No index block yet assigned

    fill(m_pad.begin(), m_pad.end(), 0);    // fill in the padding space
}

//! Check that a TFileHead read from disk has a chance of being acceptable
int TFileHead::Verify() const
{
    if ((m_chan != 0xffff) ||                       // correct ident for header
        (TFileHeadID(m_doParent).m_Major > REV_MAJOR) || // cannot cope with this
        (!TFileHeadID(m_doParent).IdentOK()) ||     // File ident wrong
        (m_nUserStart < sizeof(TFileHead)) ||       // user start in wrong place
        (m_nChanStart < m_nUserStart+m_nUserSize) ||
        (m_nChannels > MAXCHANS) ||                 // Check channel count
        (m_nChannels < MINCHANS) ||
        (m_nStringStart < m_nChanStart+m_nChannels*m_nChanHeadSize) ||
        (m_nChanHeadSize != sizeof(TChanHead)) ||
        (m_doNextIndex > m_doNextBlock) ||
        (m_nHeaderExt > HD_EXT))                    // not too many extensions
        return WRONG_FILE;

    // Check that the string table starts within the header
    uint32_t maxOffset = DBSize + m_nHeaderExt * (DBSize-DBHSize);
    if (m_nStringStart >= maxOffset)
        return WRONG_FILE;
    return S64_OK;
}

//! Get the version major and minor number held in the file head.
/*!
 The first major number is 1. This allows us to tell the difference between an old
 file opened up to pretend to be a 64-bit file and a new one.
*/
int TFileHead::Version() const
{
    const TFileHeadID id(m_doParent);
    return (id.m_Major << 8) | id.m_Minor;
}

//---------------------- TSon64File ------------------------------------------------
// Call to allocate the next disk block of size DBSize bytes at the end of the file.
// The first block is always allocated to the head when a file is created, so this can
// never return 0 (unless the file is full... unlikely with current disk systems!).
// YOU MUST HOLD m_mutHead TO DO THIS.
TDiskOff TSon64File::LockedAllocateDiskBlock()
{
    TDiskOff doReturn = m_Head.m_doNextBlock;   // next block to be written
    if (doReturn)                               // 0 means file is full!
    {
        m_Head.m_doNextBlock += DBSize;         // increase for the next user
        m_bHeadDirty = true;                    // we have made a change
    }
    return doReturn;
}

//! Allocate the next free disk block
/*!
\internal
Call to allocate the next disk block of size DBSize bytes at the end of the file.
The first block is always allocated to the head when a file is created, so this can
never return 0 (unless the file is full... unlikely with current disk systems!).
You must NOT be holding m_mutHead or you will deadlock here.
\return Offset to the block or 0 if the file/disk is full.
*/
TDiskOff TSon64File::AllocateDiskBlock()
{
    THeadLock lock(m_mutHead);
    return LockedAllocateDiskBlock();
}

//! Allocate the next index block
/*!
\internal
Call this to allocate the next lookup block on the disk. We reserve a disk block
of space and then sub-allocate it. You must NOT be holding the head mutex or you
will deadlock here.
\return Offset to the block or 0 if the file/disk is full.
*/
TDiskOff TSon64File::AllocateIndexBlock()
{
    THeadLock lock(m_mutHead);
	if (m_Head.m_doNextIndex == 0)				// if no index block allocated
	{
		m_Head.m_doNextIndex = LockedAllocateDiskBlock();	// next block to allocate
		// If being cautious, we could zero this block in case we collapse as
		// this block will have random stuff in it until all used.
	}

	TDiskOff doReturn = m_Head.m_doNextIndex;	// return value
	if (doReturn)
	{
		if ((m_Head.m_doNextIndex & DLMask) == DLMask)// assigning the last one?
			m_Head.m_doNextIndex = 0;			// we have used all index sub-blocks
		else
			m_Head.m_doNextIndex += DLSize;		// on to the next sub-block
        m_bHeadDirty = true;                    // we have made a change
	}
	return doReturn;
}

//! Extend the file head space to at least the size passed in, if necessary.
/*!
\internal
You _must_ hold m_mutHead to do this. Any new blocks are filled with 0s apart
from the block headers which are set to indicate they are part of the file head.
\param nNewSize The desired size. If this exceeds the space already allocated and
                more space is available and the file is not read only, we will
                attempt expansion. 
\return         true if done (or nothing to do), false if not possible or error. 
*/
bool TSon64File::ExtendHeadSpace(unsigned int nNewSize)
{
	if (nNewSize <= DBSize)						// if only in header...
		return true;							// ...we are done

    const unsigned int nExtSize = DBSize - DBHSize;
	unsigned int nExpBlock = 1 + (nNewSize - DBSize - 1) / nExtSize;
	if (nExpBlock <= m_Head.m_nHeaderExt)		// if no expansion is needed...
		return true;							// ...we are done

    if ((nExpBlock > HD_EXT) || m_bReadOnly)    // if too much, or read only...
        return false;                           // ...do nothing and fail

    // We need to allocate additional expansion blocks, so create one to write
	TExpBlock ExpBlock;

	// Write header space as zero filled (so unused space has known content?)
	while (m_Head.m_nHeaderExt < nExpBlock)
	{
		TDiskOff doPos = LockedAllocateDiskBlock();	// where expansion is to be written
		m_Head.m_HeaderExt[m_Head.m_nHeaderExt] = doPos;
		ExpBlock.Init(0, 0xffff);				// 0xffff is header channel ID
		if (Write(&ExpBlock, DBSize, doPos) < 0)
			return false;
		++m_Head.m_nHeaderExt;
        m_bHeadDirty = true;                    // Header needs writing
	}

	return true;
}

//! Convert a linear header access into header block accesses
/*!
\internal
We split the file header across header blocks that can be scattered through the file.
This takes a transfer offset and size in the "linear" domain as seen by the programmer
and converts it into block accesses. Used for both read and write. You _must_ hold
m_mutHead to call this.
\param osHead   The offset into the head where we want to read/write.
\param nRequest The total size of the transfer desired.
\param vXfer    A vector of transfers, one per intersected block, that are generated
                to match the transfer requested by osHead and nRequest.
\param bExtend  If true, we are allowed to extend the head to make room, else going
                beyond currently allocated blocks is an error.
\return         true if this access is OK, false if it is not.
*/
bool TSon64File::HeadOffset(uint32_t osHead, uint32_t nRequest, TVXfer& vXfer, bool bExtend)
{
    vXfer.clear();                      // make sure we have nothing at the start
    const unsigned int nExtSize = DBSize - DBHSize;
	uint32_t last = osHead + nRequest;	// requested size
	if (last > m_Head.MaxSpace())		// if we exceed the header size
	{
		if (!bExtend || !ExtendHeadSpace(last))
			return false;
	}

    unsigned int nBlock = 0;            // the extended block to consider
    if (osHead < DBSize)                // do we start in the first block (always allocated, not in list)
    {
        uint32_t nUse = min(DBSize - osHead, nRequest);    // size of this request
        vXfer.push_back(xfer(osHead, nUse));
        nRequest -= nUse;               // remaining data to transfer
    }
    else
    {
        // Find the extended block number in which this request starts
        osHead -= DBSize;               // make into offset into extended blocks
        nBlock = osHead/nExtSize;       // extended block index
        osHead -= (nBlock*nExtSize);    // offset into this block
        uint32_t nUse = min(nExtSize - osHead, nRequest);    // size of this request
		vXfer.push_back(xfer(m_Head.m_HeaderExt[nBlock]+DBHSize+osHead, nUse));
        ++nBlock;                       // next block we will use if we continue
        nRequest -= nUse;               // remaining data to transfer
    }

	// Now just break the transfers up into extended blocks until request done
    while (nRequest > 0)
	{
        uint32_t nUse = min(nExtSize, nRequest);    // size of this request
		vXfer.push_back(xfer(m_Head.m_HeaderExt[nBlock]+DBHSize, nUse));
        ++nBlock;                       // next block we will use if we continue
        nRequest -= nUse;               // remaining data to transfer
	}
    return true;
}

//! Read the file header
/*!
\internal
The user models the header as being contiguous data accessed via ReadHeader() and
WriteHeader() that includes the header at the very start of the file. The header is
actually stored in a sequence of blocks that may be scattered over the file. It is
an error to read beyond the currently allocated header blocks.
\param pBuffer  Target buffer to read data into.
\param bytes    The number of bytes to read. This must be matched, else a read error.
\param hOffset  The offset into the header. If the header is split across discontiguous
                blocks we hide this. For the purposes of this routine, the header appears
                to be contiguous.
\return         S64_OK (0) or a negative error code.
*/
int TSon64File::ReadHeader(void* pBuffer, uint32_t bytes, uint32_t hOffset)
{
	TVXfer vXfer;						// to get the sections to break into
    if (!HeadOffset(hOffset, bytes, vXfer))
        return PAST_EOF;                // read past end of header section
    auto it = vXfer.cbegin();
    while (it != vXfer.cend())
    {
        int iErr = Read(pBuffer, it->m_nUse, it->m_os);
        if (iErr < 0)
            return iErr;
        pBuffer = static_cast<char*>(pBuffer) + it->m_nUse;
        ++it;
    }

    return S64_OK;
}


//! Write to the file header
/*!
\internal
The user models the header as being contiguous data accessed via ReadHeader() and
WriteHeader() that includes the header at the very start of the file. The header is
actually stored in a sequence of blocks that may be scattered over the file. If you
write beyond the current end, we extend the list of blocks and we book another in the
file. As you are trashing the header, you must hold m_mutHead to use this.
\param pBuffer  Buffer holding the data to be written.
\param bytes    The number of bytes to write.
\param hOffset  The offset into the header. If the header is split across discontiguous
                blocks we hide this. For the purposes of this routine, the header appears
                to be contiguous.
\return         S64_OK (0) or a negative error code.
*/
// 
int TSon64File::WriteHeader(const void* pBuffer, uint32_t bytes, uint32_t hOffset)
{
    if (m_bReadOnly)
        return READ_ONLY;
	TVXfer vXfer;						// to get the sections to break into
    if (!HeadOffset(hOffset, bytes, vXfer, true))   // allow to extend
        return PAST_EOF;                // read past end of header section
    auto it = vXfer.cbegin();
    while (it != vXfer.cend())
    {
        int iErr = Write(pBuffer, it->m_nUse, it->m_os);
        if (iErr < 0)
            return iErr;
        pBuffer = static_cast<const char*>(pBuffer) + it->m_nUse;
        ++it;
    }

    return S64_OK;
}

//! Write the string store into the file head. You must hold the head mutex.
/*!
\return S64_OK (0) if all done OK or a negative error code.
*/
int TSon64File::WriteStringStore()
{
    if (m_bReadOnly)
        return READ_ONLY;
    uint32_t n = m_ss.BuildImage(nullptr);  // calculate space needed
    vector<uint32_t> image(n);              // allocate space
    image[0] = n;                           // set first element to the size
    m_ss.BuildImage(&image[0]);
    int err = WriteHeader(&image[0], n*sizeof(uint32_t), m_Head.m_nStringStart);
    if (err == 0)                           // if all OK...
        m_ss.SetModified(false);            // ...string_store is now saved
    return err;
}

//! Read the string store from the file head. You must hold the head mutex.
/*!
\internal
\return S64_OK (0) if all done OK or a negative error code.
*/
int TSon64File::ReadStringStore()
{
    m_ss.clear();                           // empty the store
    uint32_t n;
    int err = ReadHeader(&n, sizeof(uint32_t), m_Head.m_nStringStart);
    if (err)
        return err;

    // Now check the size. Work out the maximum head size based on header
    // extensions and then look at available space.
    uint32_t max = DBSize + m_Head.m_nHeaderExt*(DBSize-DBHSize);
    const unsigned int nNeeded = m_Head.m_nStringStart + (2 + n) * sizeof(uint32_t);
    if (nNeeded > max)                      // If the size exceeds the space...
    {
        // There is either something very wrong with this file, or we have hit a bug in
        // revision 1.0 that could caused m_nHeaderExt to not be updated, usually when
        // exporting a file with more string space than will fit (usually lots of channels).
        TFileHeadID id = m_Head;            // Get the file header
        if ((id.m_Major == 1) && (id.m_Minor == 0))
        {
            // Work out the correct header size (rely on read past end error if this is wrong)
            const unsigned int nExtSize = DBSize - DBHSize; // data bytes in each extension block
            unsigned int nExpBlock = 1 + (nNeeded - DBSize - 1) / nExtSize;
            if ((nExpBlock > m_Head.m_nHeaderExt) && (nExpBlock <= HD_EXT))
            {
                m_Head.m_nHeaderExt = nExpBlock; // this should fix the problem
                m_bHeadDirty = true;        // we need to update this (if allowed)
            }
            else
                return CORRUPT_FILE;
        }
        else
            return CORRUPT_FILE;
    }

    vector<uint32_t> image(n);              // space to read the size
    err = ReadHeader(&image[0], n*sizeof(uint32_t), m_Head.m_nStringStart);
    if (err)
        return err;

    // The maximum number of references to a string is 3 per channel plus 8 for the file
    // comment (if all strings were identical).
    if (!m_ss.LoadFromImage(&image[0], NUMFILECOMMENTS + 3 * m_Head.m_nChannels))
        return CORRUPT_FILE;

   return 0;
}

// You will already hold the relevant channel mutex to call this, or it will not
// matter (as when creating a file). We are calling from the channel, so channel
// number MUST be correct (hence only an assert).
int TSon64File::WriteChanHeader(TChanNum chan)
{
    THeadLock lock(m_mutHead);
    assert((chan < m_vChanHead.size()) && m_vChan[chan]);
    return WriteHeader(&m_vChanHead[chan], sizeof(TChanHead),
                        m_Head.m_nChanStart + sizeof(TChanHead)*chan);
}

//! Get reference to the channel header
/*!
\internal
This is here so that the S64Fix program can get access to the channel header. It
is NOT for use by normal users of the library. You MUST only use this for channels
that exist.
\param n    The channel number (assumed in range) to get information for.
\return     A reference to the channel header. Use with care.
*/
TChanHead& TSon64File::ChanHead(TChanNum n)
{
    assert(n < m_vChanHead.size());     // programming error if not the case
    return m_vChanHead[n];
}

//----------------------------------------------------------------------------------------
// Reading and writing the user-defined area. The extra data is between the file header
// and the channel data.
int TSon64File::SetExtraData(const void* pData, uint32_t nBytes, uint32_t nOffset)
{
    THeadLock lock(m_mutHead);              // acquire file head mutex
	if ((nOffset > m_Head.m_nUserSize) ||
		(nBytes > m_Head.m_nUserSize) ||
		(nOffset + nBytes > m_Head.m_nUserSize))
		return NO_EXTRA;
	return WriteHeader(pData, nBytes, nOffset + m_Head.m_nUserStart);
}

int TSon64File::GetExtraData(void* pData, uint32_t nBytes, uint32_t nOffset)
{
    THeadLock lock(m_mutHead);              // acquire file head mutex
	if ((nOffset > m_Head.m_nUserSize) ||
		(nBytes > m_Head.m_nUserSize) ||
		(nOffset + nBytes > m_Head.m_nUserSize))
		return NO_EXTRA;
	return ReadHeader(pData, nBytes, nOffset + m_Head.m_nUserStart);
}

uint32_t TSon64File::GetExtraDataSize() const
{
    THeadLock lock(m_mutHead);              // acquire file head mutex
    return m_Head.m_nUserSize;
}

//! Fill in any user data area with zeros
/*!
\internal
If the user has defined a private data are, fill it with zeros.
*/
int TSon64File::ZeroExtraData()
{
    int err = 0;
    if (m_Head.m_nUserSize)
    {
        const uint32_t maxbuf = 32768;
        uint32_t nBufferSize = (m_Head.m_nUserSize > maxbuf) ? maxbuf : m_Head.m_nUserSize;
        vector<char> buffer(nBufferSize);          // create a buffer
        fill_n(buffer.begin(), nBufferSize, 0);    // fill with 0's
        uint32_t nRemaining = m_Head.m_nUserSize;
        uint32_t nOffset = m_Head.m_nUserStart;
        while ((nRemaining > 0) && (err == 0))
        {
            uint32_t nWrite = nRemaining > maxbuf ? maxbuf : nRemaining;
            err = WriteHeader(&buffer[0], nWrite, nOffset);
            nRemaining -= nWrite;
            nOffset += nWrite;
        }
    }
    return err;
}
