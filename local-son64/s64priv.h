// s64priv.h
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

#ifndef __S64PRIV_H__
#define __S64PRIV_H__
//! \file s64priv.h
//! \brief Implementation header of the 64-bit file system

#include "s64.h"

#include <array>
#include <vector>
#include <list>
#include <string>
#include <memory>

#include <thread>
#include <mutex>
#include <shared_mutex>

//! NOFILE_ID is whatever an invalid file ID looks like in this system
#if   S64_OS == S64_OS_WINDOWS
#define NOFILE_ID INVALID_HANDLE_VALUE
#elif S64_OS == S64_OS_LINUX
#define NOFILE_ID -1
#endif

#include "s64ss.h"

//! The DllClass macro marks objects that are visible outside the library
#if   S64_OS == S64_OS_WINDOWS
#ifndef S64_NOTDLL
#ifdef DLL_SON64
#define DllClass __declspec(dllexport)
#else
#define DllClass __declspec(dllimport)
#endif
#endif
#elif S64_OS == S64_OS_LINUX
#ifndef S64_NOTDLL
#ifdef GNUC
    #define DllExport __attribute__((dllexport))
    #define DllImport __attribute__((dllimport))
#else
    #define DllExport
    #define DllImport
#endif
#endif
#endif

#ifndef DllClass
#define DllClass
#endif

namespace ceds64
{
    class TSon64File;
    class CSon64Chan;
    class CSFilter;

    //! Constants defining file system sizes
    /*!
        The size of the basic data block used for channel data is determined by (1 << #DBSizeSh).
        Practical values for #DBSizeSh are 15 (32 kB), 16 (64 kB) and 17 (128 kB). The standard
        size is 64 kB. There are also constraints due to the need to 
        The old SON32 system used 32k blocks.

        You may want to set smaller sizes for testing purposes to show that multiple-level lookups
        are working correctly (though having a smaller DLSizeSh works well for this).
    */
    enum
    {
        DBSizeSh = 16,                  //!< 1 << DbSizeSh is DBSize (standard value 16 = 64 kB)
        DBSizeShLo = 15,                //!< The minimum possible block size we allow (32 kB)
        DBSizeShHi = 26,                //!< The maximum possible block size allowed (64 MB, arbitrary)
        DBSize = (1 << DBSizeSh),       //!< bytes in a disk block (standard value is 64 kB)

		DLSizeSh = 12,				    //!< 1 << DLSizeSh is DLSize (standard value is 12)
        DLSizeShLo = 12,                //!< smallest lookup table size we allow (255 elements)
        DLSizeShHi = 16,                //!< largest lookup table allowed (16 kB, arbitrary)
        DLSize = (1 << DLSizeSh),       //!< Lookup table disk block size (standard value is 4096)
        DLUperDB = DBSize / DLSize,     //!< Look up tables in a disk block (standard value is 16)
        DLMask = (DLUperDB-1)*DLSize,   //!< Mask to test if used all lookups in an Index block
        HD_EXT = 128,                   //!< maximum header extensions (128)
        FHComments = NUMFILECOMMENTS,   //!< number of file header comments (8)
    };

    static_assert(DBSizeSh >= DBSizeShLo, "Data blocks are too small, even for testing");
    static_assert(DBSizeSh <= DBSizeShHi, "Data blocks are too large");
    static_assert(DLSizeSh >= DLSizeShLo, "Lookup blocks are too small, even for testing");
    static_assert(DLSizeSh <= DLSizeShHi, "Lookup blocks are too large");
    static_assert(DLSizeSh <= DBSizeSh,   "Lookup table larger than disk block");
    static_assert(DLSizeSh+3 <= DBSizeSh, "Not enough space to pack in index and level bits");

    //! Structure used to identify the file and revision
    /*!
        The first three bytes of the data file hold "S64". Then next two hold codes for
        the size of the data block being 'a' - 1 + DBSizeSh followed by the size of the
        lookup table block as 'a' - 1 + DLSizeSh. The last byte is 0.
        
        This means that with a 64k data block and a 4kB lookup block you have the text:
        "S64pl" at the start of the file.
    */
    struct DllClass TFileHeadID
    {
        std::array<char, 6> m_Ident;    //!< "S64xy<0>" file ID - x,y code for data and LU size
        uint8_t     m_Minor;            //!< Minor version revision
        uint8_t     m_Major;            //!< Major version revision
        TFileHeadID(uint8_t major, uint8_t minor);  //!< Construct a header
        explicit TFileHeadID(TDiskOff offset);      //!< to convert from TDiskBlockHead
        operator const TDiskOff() const //!< to convert into a TDiskOff
        {
            return *reinterpret_cast<const TDiskOff*>(this);
        };
        bool IdentOK(int32_t* pBlkSz = nullptr, int32_t* pLUpSz = nullptr) const;
    };
    static_assert(sizeof(TDiskOff) == sizeof(TFileHeadID), "TDiskOff or TFileHeadID size problem");


    //! One item in TDiskLookup
    /*!
     All disk blocks and index blocks have a header followed immediately by
     the first time. This means that as long as the block is not empty, you
     can always read the first time with common code.
    */
    struct TDiskTableItem
    {
        TSTime64    m_time;     //!< The time of the first item in the block
        TDiskOff    m_do;       //!< The disk offset of the item

        /*!
        \brief Construct a table entry
        \param t    The time to associate with the entry.
        \param d    The disk offset of the table entry
        */
        explicit TDiskTableItem(TSTime64 t=0, TDiskOff d=0)
            : m_time( t )
            , m_do( d )
        {}

        bool operator<(const TDiskTableItem& rhs) const //!< So we can search items
        {
            return m_time < rhs.m_time;
        }
        bool operator<(TSTime64 t) const {return m_time < t;} //!< So we can search by time
    };

    // To calculate the sizes needed for the index etc we rely on fixed sizes for the
    // TDiskTableItem and TDiskBlockHead (both must be 16 bytes).
    static_assert(sizeof(TDiskTableItem) == 16, "TDiskTableItem is wrong size");
//    bool operator<(const TDiskTableItem& lhs, const TDiskTableItem& rhs);


    //! 16-byte header for each disk block
    /*!
	  Blocks on disk are either of size DBSize or DLSize. Both are powers of 2 with DBSize
	  typically 64 kB and DLSize 4k. DBSize blocks are always allocated on DBSize file offsets;
	  DLSize blocks are grouped together in groups of DBSize/DLSize blocks, allocated from a
      DBSize area which is aligned at a multiple of DBSize bytes from the start of the file.

      Each DBSize and DLSize block has a 16-byte header.
	  The first 8 bytes of the header hold the disk offset of the parent block with the lower bits
	  of the disk offset holding additional information which are masked off when getting the
      parent block disk offset (see below). The parent block of a data block is always
	  an index block. The parent block of an index block is either another index block or if it
      is the first index block of a channel, it holds 0.

      Calculation of the masks
      ------------------------
      The smallest block used for addressing memory is DLSize, which is
      always <= DBSize and both are powers of 2. This means that any address we use has at least
      DLSizeSh 0's at the bottom. We use this extra space to store the index into the parent block
      lookup table at the low end, followed by the block level, then there should be some 0's.
      
      Each lookup item occupies 16 bytes, so there are a maximum of (1 << (DLSizeSz-4))
      items in the index. However, the first item is actually a header, so there are 1 less than 
      this. The mask for the index part is IndexMask (below). With 64k data blocks and 4k lookup
      blocks this gives us a LevelSh of 8 and a IndexMask of 0xff. With these setting we get 1 spare
      0 bit. If we changed to DLSize of 13, then there is 1 more bit of IndexMask and no spare bits,
      so DLSizeSh+3 cannot exceed DBSizeSh. There is a static_assert to check for this.
    */
    struct TDiskBlockHead
    {
        //! Masks and shifts used to extract the offset, parent index and level
        enum eDBH
        {
            LevelSh = DLSizeSh - 4,         //!< (m_doParent >> LevelSh) & LevelMask ->level (standard value 8)
            LevelMask = 7,                  //!< Maximum possible levels with standard settings is 6
            IndexMask = ((1 << LevelSh)-1), //!< (m_doParent & IndexMask) -> parent index (standard value 0xff)

        };
        TDiskOff    m_doParent; //!< The parent block, level and parent block index.
        /*!
        Unless this is the top level index for a channel when this is 0, this holds the offset
        to the parent index block in the data file with the bottom bits holding additional
        information.

        The bottom LevelSh bits of this value (which is 8 bits in the standard case) hold the
        index into the parent index block that points back at this block. This information can
        be reconstructed dynamically, but is it is useful when verifying that a file is not
        damaged. IndexMask is the mask for these bits.
        
        The next 3 bits hold the level of this block. Level 0 is data, levels 1-6 (as
        level 7 cannot be reached with standard settings - the file would be too large) are
        the index level. Level 1 is the index that points at the data blocks.
        */
        uint16_t    m_chan;     //!< The channel number (0-0xfffe) or 0xffff for file header blocks.
        /*!
        The maximum number of channels in a file is arbitrarily limited to MAXCHANS (1000) and
        could be increased, if required.
        */
        TChanID     m_chanID;   //!< Channel re-use count. 0 initially, used to help recover data.
        /*!
        We store a channel ID in TChanHead::m_chanID that starts at 0 when a file is created and
        that increments each time the channel is reused. Each time we write a reused data block
        we set this value to match. When recovering a file we can then tell the difference between
        blocks that belong to the current use of the channel, and blocks from previous uses that
        have not yet be reused.
        
        Yes, this becomes ambiguous if a channel is partially reused 65536 times. However, this
        is only a problem for data recovery, not for general use as we know how many blocks are
        currently in used. Once a channel is completely reused, all blocks have the same reuse
        ID, so you have another 65536 reuses before it could be ambiguous.
        */
        uint32_t    m_nItems;   //!< Number of valid items in this block

        operator const TFileHeadID() const  //!< Convert to TFileHeadID
        {
            return TFileHeadID(m_doParent);
        }

        TDiskOff GetParentBlock() const     //!< Get parent block address
        {
            return (m_doParent >> DLSizeSh) << DLSizeSh;
        }

        unsigned int GetLevel() const       //!< Get block level (0=data, 1 up for index)
        {
            return (((uint32_t)m_doParent) >> LevelSh) & LevelMask;
        }

        unsigned int GetParentIndex() const //!< Get index of this block in the parent index
        {
            return ((uint32_t)m_doParent) & IndexMask;
        }

        void SetParent(TDiskOff doParent, unsigned int index, unsigned int level);
        bool SetParentIndex(unsigned int index);

        //! Initialise the block
        /*!
        This is used by CIndex, by TFileHead and when creating file header expansion blocks.
        \param doParent This disk offset of the parent block. It is also used for the file
                        format revision when creating the first block of the file.
        \param chan     The channel this belongs to or 0xffff for a header block.
        */
        void Init(TDiskOff doParent, uint16_t chan)
        {
            m_doParent = doParent;
            m_chan = chan;
            m_chanID = 0;
            m_nItems = 0;
        }

        bool operator==(const TDiskBlockHead& rhs) const    //!< Test all fields for equality
        {
            return (m_doParent == rhs.m_doParent) &&
                (m_chan == rhs.m_chan) &&
                (m_chanID == rhs.m_chanID) &&
                (m_nItems == rhs.m_nItems);
        }

        bool operator!=(const TDiskBlockHead& rhs) const    //!< Test all fields for inequality
        {
            return !operator==(rhs);
        }
    };

    // To work out the masks and shifts above, we rely on these sizes being the same
    static_assert(sizeof(TDiskBlockHead) == sizeof(TDiskTableItem), "TDiskBlockHead, TDiskTableItem sizes differ");

    enum                        // hacks for sizes
    {
        DBHSize = sizeof(TDiskBlockHead),                   //!< Size of disk block header (16 bytes)
        DLUItems = (DLSize-DBHSize)/sizeof(TDiskTableItem), //!< Items in a lookup table (standard value 255)
    };


    //! 0x800 byte header for the file
    /*!
    This header identifies that this is a SON64 file, optionally identifies the
    application that created the file and holds the basic parameters that define
    the file structure.
    */
    struct TFileHead : public TDiskBlockHead
    {
        TCreator    m_creator;          //!< identifies the creating application
        TTimeDate   m_tdZeroTick;       //!< the time and date associated with tick 0
        double      m_dSecPerTick;      //!< Seconds per tick. May reconsider and use nS, or ratio of 64-bit values

        uint32_t    m_nUserSize;        //!< Size of the user area in bytes
		uint32_t	m_nUserStart;		//!< Byte offset to the user area in the file header
		uint32_t	m_nChanStart;		//!< Byte offset to the channel area in the file header
		uint32_t	m_nStringStart;		//!< Byte offset to the string table on disk
        uint32_t    m_nChannels;        //!< number of channels this file has headers for
        uint32_t    m_nChanHeadSize;    //!< bytes per channel header (multiple of 8, please)
        std::array<s64strid, FHComments> m_comments; //!< Space for comments, being the index in the string table.
		std::array<uint32_t, 225> m_pad;     //!< Pad the header to 0x800 bytes

        uint32_t    m_nHeaderExt;       //!< number of extra header blocks used
        TDiskOff    m_doNextBlock;      //!< Next position to write to in the file
        TDiskOff    m_doNextIndex;      //!< 0 if not assigned, next part of partially used index
        TSTime64    m_maxFTime;         //!< -1 if unset, else maximum time of item in the file
        std::array<TDiskOff, HD_EXT> m_HeaderExt;    //!< Extended header blocks or 0 if not used

        void Init(uint16_t nChannels, uint32_t nFUser);
		uint32_t MaxSpace() const       //!< The Maximum data space available in the file header
        {
            return DBSize + m_nHeaderExt*(DBSize-DBHSize);
        }
        int Verify() const;             // simple checks on read to see if valid
        int Version() const;            // get the file version
    };

    static_assert(sizeof(TFileHead) == 0x800, "sizeof(TFileHead) not 0x800");

    //! Flags used in TChanHead::m_flags
    enum 
    {
        ChanFlag_LevelHigh = 1,         //!< Set for level channel to indicate init state
    };

    //! The channel header as stored on disk.
    /*!
     Note that the items in this are arranged so that they pack without leaving
     gaps. Once we ship the library we cannot change the headers without breaking
     backwards compatibility, but we do allow the m_pad space, which is initialised
     to zero for future expansion without the need to rewrite code.
    */
    struct TChanHead
    {
        TDiskOff    m_doIndex;          //!< File offset of top level TDiskLookup or 0 if none
        TSTime64    m_lastTime;         //!< Last time in file (if m_nBlocks > 0)
        uint64_t    m_nBlocks;          //!< Number of active data blocks for the channel
        uint64_t    m_nAllocatedBlocks; //!< Set to total blocks when channel deleted or reused

        uint32_t    m_nObjSize;         //!< size of the stored object (event-based or wave)
        uint16_t    m_nRows;            //!< attached items rows (for extended markers)
        uint16_t    m_nColumns;         //!< attached items columns (used by AdcMark)
        uint16_t    m_nPreTrig;         //!< used in AdcMark for pre-trigger points
        uint16_t    m_nItemSize;        //!< m_nRows x m_nColumns of things this size
        TChanID     m_chanID;           //!< initially 0, incremented when we re-use

        TDataKind   m_chanKind;         //!< channel type (8-bit, so packs nicely)
        TDataKind   m_lastKind;         //!< when deleted, last type is saved here for undelete

        int32_t     m_iPhyCh;           //!< physical channel number (-1 if not set)
        s64strid    m_title;            //!< the channel title string
        s64strid    m_units;            //!< channel units (multiple separated by |)
        s64strid    m_comment;          //!< channel comment

        TSTime64    m_tDivide;          //!< time per point (wave), WaveMark
        double      m_dRate;            //!< expected item rate (for buffer allocation)
        double      m_dScale;           //!< wave channel scale factor: val = short*scale/6553.6 + offset 
        double      m_dOffset;          //!< wave channel offset: val = short*scale/6553.6 + offset
        double      m_dYLow;            //!< suggested low value for y axis
        double      m_dYHigh;           //!< suggested high value for y axis

        uint64_t    m_flags;            //!< flags space. Currently only bit 0 = level starts high
        std::array<uint64_t, 19> m_pad; //!< space for the future that is initialised to 0

        TChanHead();
		void ResetForReuse();           //!< set a deleted channel for reuse
        void EmptyForReuse();           //!< set a used channel back to empty, preserving information
        void IncReusedBlocks();         //!< Increment use count when reusing, may end reusing
        bool Delete();                  //!< delete channel, return true if a change
        bool IsDeleted() const          //!< True if this is a deleted channel
        {
            return (m_lastKind != ChanOff) && (m_chanKind == ChanOff);
        }
        bool IsUnused() const           //!< True if this channel has never been used
        {
            return (m_lastKind == ChanOff) && (m_chanKind == ChanOff);
        }
        bool IsUsed() const             //!< True if this channel is currentl in use
        {
            return m_chanKind != ChanOff;
        }
        bool ReusingBlocks() const      //!< True if we are reusing blocks from a previous channel use.
        {
            return m_nAllocatedBlocks > m_nBlocks;
        }
        int Undelete();
    };

    //! The object that implements a 64-bit SON data file
    /*!
    This class defines the user interface to the data files. This is the native
    format of the library, so all library features work for this file type.
    
    \warning The declarations of each exported function include DllClass, which
    expands to `__declspec(dllexport)` and `__declspec(dllimport)`. It is tempting
    to mark the entire class DllClass qnd remove DllClass from each function.
    _Do not_ do this. Setting `class DllClass TSon64File` will export everything,
    including all the private members and associated types. This includes the
    boost and std library objects (vector, etc). Having these exported causes
    bloat and may generate obscure linker problems.
    */
    class TSon64File : public CSon64File
    {
        friend class CSon64Chan;
        friend class CBlockManager;

        // Items from here on are part of the defined interface CSon64File
    public:
        DllClass TSon64File();
        virtual DllClass ~TSon64File();
#ifdef _UNICODE
        virtual DllClass int Create(const wchar_t* szName, uint16_t nChannels, uint32_t nFUser = 0);
        virtual DllClass int Open(const wchar_t* szName, int iOpenMode = 1, int flags = ceds64::eOF_none);
#endif
        virtual DllClass int Create(const char* szName, uint16_t nChannels, uint32_t nFUser = 0);
        virtual DllClass int Open(const char* szName, int iOpenMode = 1, int flags = ceds64::eOF_none);
        virtual DllClass bool CanWrite() const { return !m_bReadOnly; }
        virtual DllClass int Close();
        virtual DllClass int EmptyFile();
        virtual DllClass int GetFreeChan() const;
        virtual DllClass int Commit(int flags = 0);
        virtual DllClass bool IsModified() const;
        virtual DllClass int FlushSysBuffers();

        virtual DllClass double GetTimeBase() const;
        virtual DllClass void SetTimeBase(double dSecPerTick);
        virtual DllClass int SetExtraData(const void* pData, uint32_t nBytes, uint32_t nOffset);
        virtual DllClass int GetExtraData(void* pData, uint32_t nBytes, uint32_t nOffset);
        virtual DllClass uint32_t GetExtraDataSize() const;
        virtual DllClass int SetFileComment(int n, const char* szComment);
        virtual DllClass int GetFileComment(int n, int nSz = 0, char* szComment = nullptr) const;
        virtual DllClass int MaxChans() const {return m_Head.m_nChannels;}
        virtual DllClass int AppID(TCreator* pRead, const TCreator* pWrite = nullptr);
        virtual DllClass int TimeDate(TTimeDate* pTDGet, const TTimeDate* pTDSet = nullptr);
        virtual DllClass int GetVersion() const;
        virtual DllClass uint64_t FileSize() const;
        virtual DllClass uint64_t ChanBytes(TChanNum chan) const;
        virtual DllClass TSTime64 MaxTime(bool bReadChans = true) const;
        virtual DllClass void ExtendMaxTime(TSTime64 t);

        virtual DllClass TDataKind ChanKind(TChanNum chan) const;
        virtual DllClass TSTime64 ChanDivide(TChanNum chan) const;
        virtual DllClass double IdealRate(TChanNum chan, double dRate = -1.0);
        virtual DllClass int PhyChan(TChanNum) const;
        virtual DllClass int SetChanComment(TChanNum chan, const char* comment);
        virtual DllClass int GetChanComment(TChanNum chan, int nSz = 0, char* szComment = nullptr) const;
        virtual DllClass int SetChanTitle(TChanNum chan, const char* szTitle);
        virtual DllClass int GetChanTitle(TChanNum chan, int nSz = 0, char* szTitle = nullptr) const;
        virtual DllClass int SetChanScale(TChanNum chan, double dScale);
        virtual DllClass int GetChanScale(TChanNum chan, double& dScale) const;
        virtual DllClass int SetChanOffset(TChanNum chan, double dOffset);
        virtual DllClass int GetChanOffset(TChanNum chan, double& dOffset) const;
        virtual DllClass int SetChanUnits(TChanNum chan, const char* szUnits);
        virtual DllClass int GetChanUnits(TChanNum chan, int nSz = 0, char* szUnits = nullptr) const;
        virtual DllClass TSTime64 ChanMaxTime(TChanNum chan) const;
        virtual DllClass TSTime64 PrevNTime(TChanNum chan, TSTime64 tStart, TSTime64 tEnd = 0,
                                   uint32_t n = 1, const CSFilter* pFilter = nullptr, bool bAsWave = false); 
        virtual DllClass int ChanDelete(TChanNum chan);
        virtual DllClass int ChanUndelete(TChanNum chan, eCU action=eCU_kind);
        virtual DllClass int GetChanYRange(TChanNum chan, double& dLow, double& dHigh) const;
        virtual DllClass int SetChanYRange(TChanNum chan, double dLow, double dHigh);
        virtual DllClass int ItemSize(TChanNum chan) const;

        virtual DllClass void Save(int chan, TSTime64 t, bool bSave);
        virtual DllClass void SaveRange(int chan, TSTime64 tFrom, TSTime64 tUpto);
        virtual DllClass bool IsSaving(TChanNum chan, TSTime64 tAt) const;
        virtual DllClass int NoSaveList(TChanNum chan, TSTime64* pTimes, int nMax, TSTime64 tFrom = -1, TSTime64 tUpto = TSTIME64_MAX) const;

        virtual DllClass int LatestTime(int chan, TSTime64 t);
        virtual DllClass double SetBuffering(int chan, size_t nBytes, double dSeconds = 0.0);

        virtual DllClass int SetEventChan(TChanNum chan, double dRate, TDataKind evtKind = EventFall, int iPhyCh=-1);
        virtual DllClass int WriteEvents(TChanNum chan, const TSTime64* pData, size_t count);
        virtual DllClass int ReadEvents(TChanNum chan, TSTime64* pData, int nMax, TSTime64 tFrom, TSTime64 tUpto, const CSFilter* pFilter = nullptr);

        virtual DllClass int SetMarkerChan(TChanNum chan, double dRate, TDataKind kind = Marker, int iPhyChan = -1);
        virtual DllClass int WriteMarkers(TChanNum chan, const TMarker* pData, size_t count);
        virtual DllClass int ReadMarkers(TChanNum chan, TMarker* pData, int nMax, TSTime64 tFrom, TSTime64 tUpto, const CSFilter* pFilter = nullptr);
        virtual DllClass int EditMarker(TChanNum chan, TSTime64 t, const TMarker* pM, size_t nCopy = sizeof(TMarker));

        virtual DllClass int SetLevelChan(TChanNum chan, double dRate, int iPhyChan = -1);
        virtual DllClass int SetInitLevel(TChanNum chan, bool bLevel);
        virtual DllClass int WriteLevels(TChanNum chan, const TSTime64* pData, size_t count);
        virtual DllClass int ReadLevels(TChanNum chan, TSTime64* pData, int nMax, TSTime64 tFrom, TSTime64 tUpto, bool& bLevel);

        virtual DllClass int SetTextMarkChan(TChanNum chan, double dRate, size_t nMax, int iPhyChan = -1);
        virtual DllClass int SetExtMarkChan(TChanNum chan, double dRate, TDataKind kind, size_t nRows, size_t nCols = 1, int iPhyChan = -1, TSTime64 tDvd = 0, int nPre = 0);
        virtual DllClass int GetExtMarkInfo(TChanNum chan, size_t *pRows=nullptr, size_t* pCols=nullptr) const;

        virtual DllClass int SetWaveChan(TChanNum chan, TSTime64 tDvd, TDataKind wKind, double dRate = 0.0, int iPhyCh=-1);
        virtual DllClass TSTime64 WriteWave(TChanNum chan, const short* pData, size_t count, TSTime64 tFrom);
        virtual DllClass TSTime64 WriteWave(TChanNum chan, const float* pData, size_t count, TSTime64 tFrom);
        virtual DllClass int ReadWave(TChanNum chan, short* pData, int nMax, TSTime64 tFrom, TSTime64 tUpto, TSTime64& tFirst, const CSFilter* pFilter = nullptr);
        virtual DllClass int ReadWave(TChanNum chan, float* pData, int nMax, TSTime64 tFrom, TSTime64 tUpto, TSTime64& tFirst, const CSFilter* pFilter = nullptr);

        virtual DllClass int WriteExtMarks(TChanNum chan, const TExtMark* pData, size_t count);
        virtual DllClass int ReadExtMarks(TChanNum chan, TExtMark* pData, int nMax, TSTime64 tFrom, TSTime64 tUpto, const CSFilter* pFilter = nullptr);
    
        // This is the end of the defined interface. Anything that is DllClass from here on is
        // so that it can be used by S64Fix.
    protected:
        int DllClass Read(void* pBuffer, uint32_t bytes, TDiskOff offset);
        int Write(const void* pBuffer, uint32_t bytes, TDiskOff offset);
        int ReadHeader(void* pBuffer, uint32_t bytes, uint32_t hOffset);
        int WriteHeader(const void* pBuffer, uint32_t bytes, uint32_t hOffset);
        int ZeroExtraData();
        int ResetForReuse(TChanNum chan);
        DllClass TChanHead& ChanHead(TChanNum n); // export so visible to S64Fix
        TDiskOff DllClass GetFileSize();        // not const due to mutex use

        TDiskOff AllocateDiskBlock();           // Allocate the next disk block
        TDiskOff AllocateIndexBlock();          // Allocate the next index block


    private:
        TDiskOff LockedAllocateDiskBlock();     // Head is already locked
        int WriteChanHeader(TChanNum chan);     // called from channels
        int CreateChannelFromHeader(TChanNum chan);
        int CreateChannelsFromHeaders();        // create all the channels

        struct xfer
        {
            TDiskOff m_os;
            uint32_t m_nUse;
            xfer() : m_os(0), m_nUse(0) {}
            xfer(TDiskOff os, uint32_t nUse) : m_os(os), m_nUse(nUse) {}
        };
        typedef std::vector<xfer> TVXfer;
        bool HeadOffset(uint32_t osHead, uint32_t nRequest, TVXfer& vXfer, bool bExtend=false);
        bool ExtendHeadSpace(unsigned int nNewSize);
        int WriteStringStore();
        int ReadStringStore();

        TS64FH m_file;                  // the file handle
        typedef std::lock_guard<std::mutex> TFileLock;
        std::mutex m_mutFile;           // file access mutex
        bool m_bReadOnly;               // are we read only

        TFileHead m_Head;               // the file head
        typedef std::lock_guard<std::mutex> THeadLock;
        mutable std::mutex m_mutHead;   // file head mutex
		bool m_bHeadDirty;				// true if head needs writing
        bool m_bOldFile;                // true if opened rather than created
        double m_dBufferedSecs;         // number of seconds of buffering time

        string_store m_ss;              // the string store. Uses the head lock mutex

        // This area handles the channel list. We keep the TChanHead stuff together so
        // it is efficient to write it all in one go. The two vectors have a shared
        // mutex (multiple readers, single writer). You only need to hold the write
        // lock when adding or removing channels; make this as fast as you can, please.
        std::vector<TChanHead> m_vChanHead;  // channel header storage space
        typedef std::unique_ptr<CSon64Chan> TpChan;
        std::vector<TpChan> m_vChan;         // the channel object pointers

        mutable std::shared_mutex m_mutChans; // shared mutex to the channel list
        typedef std::unique_lock<std::shared_mutex> TChWrLock;
        typedef std::shared_lock<std::shared_mutex> TChRdLock;
    };
}
#undef DllClass
#endif
