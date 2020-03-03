//s64chan.h
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
#ifndef __S64CHAN_H__
#define __S64CHAN_H__
//! \file s64chan.h
//! \brief Header for the 64-bit file system channels
//! \internal

#include "s64priv.h"
#include "s64circ.h"
#include "s64st.h"
#include "s64filt.h"
#include "s64dblk.h"

using std::string;
using std::vector;
using std::unique_ptr;
using std::array;

//! The DllClass macro marks objects that are visible outside the library
#if   S64_OS == S64_OS_WINDOWS
#ifndef S64_NOTDLL
#ifdef DLL_SON64
#define DllClass __declspec(dllexport)
#else
#define DllClass __declspec(dllimport)
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
    class CSRange;

    enum
    {
        CircBuffMinShift = 5,   //!< min move is 1/32 of buffer size
    };

    //! A lookup table of items
    /*!
     These structures are used to lookup items on the disk based on times. This is the
     structure that is saved on disk. CIndex is a wrapper for this and is held in memory.
     This is exported so S64Fix can see it. Note that the chanID held in each LU block is
     0 (it is not incremented on reuse) as lookup tables are reused,
    */
    struct DllClass TDiskLookup : public TDiskBlockHead
    {
        array<TDiskTableItem, DLUItems> m_items; //!< The list of items

        TDiskLookup();                          //!< Default constructor
        TDiskLookup(const TDiskLookup& rhs);    //!< Copy constructor
 
        int AddIndexItem(TDiskOff pos, TSTime64 time);
        unsigned int UpperBound(TSTime64 time, size_t nUse = 0) const;
        bool Verify() const;
#ifdef _DEBUG
        void Dump() const;
#endif
    };


    //! A wrapper class for TDiskLookup
    /*!
    \internal
     This is held in memory, only the TDiskLookup part is written/read as part of the
     disk file. This is a holder for the index blocks used to locate data.
    */
    class CIndex
    {
        TDiskLookup m_dlu;              //!< the lookup table
        TDiskOff    m_do;               //!< where it goes on disk

        // Next member variables are used for writing only.
        bool        m_bModified;        //!< does it need writing? (used for writing only)
		uint16_t	m_indexReuse;		//!< when re-using, the index we have reached (and are using)

    public:
        CIndex()
            : m_do( 0 )
            , m_bModified( false )
            , m_indexReuse( 0 )
        {}

        CIndex(TChanNum chan, TDiskOff doParent, unsigned int index, unsigned int level);

        void SetDiskOffset(TDiskOff pos);
        TDiskOff GetDiskOffset() const {return m_do;}   //!< Get the file offset of this TDiskLookup
        int AddIndexItem(TDiskOff pos, TSTime64 time);
        void SetParent(TDiskOff doParent, unsigned int index, unsigned int level);

        //! Utility added to help fix a bad index to fix a bug where the index was not set
        /*!
        \param index The index of this disk block in the parent index block. If this differs
        from the currenly held index, change the index.
        \return      true if the proposed index was different, else false if no change.
        */
        bool SetParentIndex(unsigned int index){ return m_dlu.SetParentIndex(index); }

        TSTime64 GetFirstTime() const;

        TDiskLookup* GetTable() {return &m_dlu;}        //!< Get the underlying TDiskLookup
        const TDiskLookup* GetTable() const { return &m_dlu; }  //!< Get the const TDiskLookup

        void ClearModified(){m_bModified = false;}      //!< Clear the modified flag for this block
        bool IsModified() const {return m_bModified;}   //!< Return the modified state of the block

        void clear();

		TDiskOff GetReuseOffset() const {return m_dlu.m_items[m_indexReuse].m_do;} //!< Get the current reuse disk offset
        void SetReuseTime(TSTime64 tFirst);

		uint16_t GetReuseIndex() const {return m_indexReuse;}   //!< Get the current reuse index
        //! Set the reuse index for this block
        /*!
        \param index The new reuse index to set.
        */
		void SetReuseIndex(uint16_t index){m_indexReuse = index;}
		bool IncReuseIndex();

        //! Implement the upper_bound algorithm to locate an index
        /*!
        Search the table for the index where a specified time would be placed.
        \param time    The time we wish to locate
        \param nUse    If non-zero, it must be <= Items() and is the number of items to search.
                       This is for the case when we are reusing a block, so all items might not
                       (yet) be used.
        \return        The index of the first item that starts at a time that is > time, or
                       returns m_nItems if no item is greater. A value of 0 means that the
                       first item is at a greater time.
        */
        unsigned int UpperBound(TSTime64 time, size_t nUse = 0) const { return m_dlu.UpperBound(time, nUse); }
        size_t GetLevel() const {return m_dlu.GetLevel();}  //!< Get the level of this block
        unsigned int GetParentIndex() const {return m_dlu.GetParentIndex();}    //!< Get the index into the parent index
        unsigned int Items() const {return m_dlu.m_nItems;} //!< Get the number of items in the index
    };

    typedef vector<CIndex> VIndex;      //!< used to hold loookup indices

    //! This manages a data block and the indices required to track it.
    /*!
    \internal
    It is used when reading data. It contains a data block and the list of indices
    that track the block on disk. These indices allow you to iterate through data in
    an efficient way. Most block iteration only requires 1 disk read (of the new block).
    Every DLUItems an extra block read is needed, every DLUItems^2 2 extra blocks, and so on.
    */
    class CBlockManager
    {
        CSon64Chan& m_chan;             //!< The channel associated with the data block
        VIndex m_vIndex;                //!< the list of index blocks to the data
        unique_ptr<CDataBlock> m_pDB;   //!< the data block
        int64_t m_nBlock;               //!< The block number, or -1 if invalid
        std::vector<uint16_t> m_vReuse; //!< number of reused items in last block at this level
    public:
        explicit CBlockManager(CSon64Chan& rChan);

        bool HasDataBlock() const {return static_cast<bool>(m_pDB);}   //!< True if we are holding memory for a data block
        //! Take ownership of the data block passed in
        /*!
        \param pBlock A data block allocated with new that we are to take ownership of. We will
                      delete any block we currently hold.
        */
        void SetDataBlock(CDataBlock* pBlock){m_pDB.reset(pBlock);}
        int LoadBlock(TSTime64 tFind);
        int NextBlock(unsigned int i = 0);
        int PrevBlock(unsigned int i = 0);
        bool Valid() const {return m_nBlock >= 0;}  //!< Test if this block is valid
        const CDataBlock& DataBlock() const {return *m_pDB;}    //!< Get a const reference to the block
        CDataBlock& DataBlock() {return *m_pDB;}    //!< Get a reference to the block
        void Invalidate(){m_nBlock = -1;}   //!< Mark block so next use must reread entire tree
        void UpdateIndex(unsigned int level, const CIndex& index);
        void UpdateData(const CDataBlock& block);
        void BlockAdded();              // Wrote a new block to the channel
        int FixIndex();                // Attempt to fix index chain if not OK
        int PatchIndex(unsigned int level, unsigned int uiParent);

        int SaveIfUnsaved();            // data can be modified, but not index blocks
        bool Unsaved() const            //!< true if an unsaved disk block with a known disk address
        {
            return m_pDB && m_pDB->Unsaved() && m_pDB->DiskOff();
        }
    private:
        int ReadIndex(CIndex& index, TDiskOff pos);
        int ReadDataBlock(TDiskOff pos);
        void CalcReuse(size_t nLevel);  // calculate reused item vector for this many levels
    };

    //! Encapsulates the concept of a data channel.
    /*!
    \internal
    This manages all the common features of data channels using virtual functions. It is
    overridden by specific channel type code for features that differ and to provide
    circular buffered versions of the channels.
    */
    class CSon64Chan
    {
        friend class CBlockManager;
        friend class TSon64File;
    protected:
        TSon64File& m_file;             //!< the owning file
        TChanHead&  m_chanHead;         //!< disk portion of channel header (not owner)
        TChanNum    m_nChan;            //!< channel number
        bool        m_bModified;        //!< true if the channel header needs writing to disk
        VIndex      m_vAppend;          //!< List of index blocks to write buffer on disk
        unique_ptr<CDataBlock> m_pWr;   //!< our write buffer, created when we write data
        CSaveTimes  m_st;               //!< Only used by buffering classes

        CBlockManager m_bmRead;         //!< read data block manager
        mutable std::mutex m_mutex;     //!< channel mutex (MUST acquire before mutHead)
        typedef std::lock_guard<std::mutex> TChanLock;  //!< Used to acquire channel mutex

    public:
        CSon64Chan(TSon64File& file, TChanNum nChan, TDataKind kind);
        virtual ~CSon64Chan();
        virtual int Delete();
        virtual int Undelete();
        virtual bool CanUndelete() const;
        virtual int ResetForReuse();
        virtual int EmptyForReuse();

        int FixIndex();
        void SetModified() { m_bModified = true; }

        virtual TDataKind ChanKind() const;
        virtual void SetTitle(const string& title);
        virtual string GetTitle() const;
        virtual void SetUnits(const string& units);
        virtual string GetUnits() const;
        virtual void SetComment(const string& comment);
        virtual string GetComment() const;
        virtual void SetPhyChan(int32_t iPhy);
        virtual int32_t GetPhyChan() const;
        virtual void SetIdealRate(double dRate);
        virtual double GetIdealRate() const;
        virtual TSTime64 ChanDivide() const;
        virtual void SetPreTrig(uint16_t nPreTrig);
        virtual int GetPreTrig() const;
        virtual size_t GetRows() const;
        virtual size_t GetCols() const;
        virtual void SetScale(double dScale);
        virtual double GetScale() const;
        virtual void SetOffset(double dOffset);
        virtual double GetOffset() const;
        virtual size_t GetObjSize() const;
        virtual int GetYRange(double& dLow, double& dHigh) const;
        virtual int SetYRange(double dLow, double dHigh);
        virtual int SetInitLevel(bool bInitLevel);

        virtual TSTime64 MaxTime() const;
        virtual TSTime64 PrevNTime(CSRange& r, const CSFilter* pFilter = nullptr, bool bAsWave = false);// = 0;

        virtual int AppendBlock(CDataBlock* pBlock);
        virtual int Commit();
        virtual bool IsModified() const;
        virtual uint64_t GetChanBytes() const;

        //=============================================================================
        // Routines to write data that are overridden in classes that implement them.

        //! Write TSTime64 data to the channel
        /*!
        This is overridden by each channel type that implements writing of event times.
        \param pData Pointer to the data to write. The data must be in ascending time order and
                     should not have repeated times (though we do not check this). It must be
                     after any existing data for this channel.
        \param count The number of data values to write.
        \return      S64_OK (0) or a negative error code.
        */
        virtual int WriteData(const TSTime64* pData, size_t count){return CHANNEL_TYPE;}

        //! Write TMarker data to the channel
        /*!
        This is overridden by each channel type that implements writing of Markers.
        \param pData Pointer to the data to write. The data must be in ascending time order and
        should not have repeated times (though we do not check this). It must be
        after any existing data for this channel.
        \param count The number of data values to write.
        \return      S64_OK (0) or a negative error code.
        */
        virtual int WriteData(const TMarker* pData, size_t count){ return CHANNEL_TYPE; }

        //! Write TExtMark data to the channel
        /*!
        This is overridden by each channel type that implements writing of extended markers.
        \param pData Pointer to the data to write. The data must be in ascending time order and
        should not have repeated times (though we do not check this). It must be
        after any existing data for this channel.
        \param count The number of data values to write.
        \return      S64_OK (0) or a negative error code.
        */
        virtual int WriteData(const TExtMark* pData, size_t count){ return CHANNEL_TYPE; }

        //! Write short waveform data to the channel
        /*!
        This is overridden by each channel type that implements writing of short waveforms.
        \param pData Pointer to the data to write.
        \param count The number of data values to write.
        \param tFrom The time of the first data value. If this overlaps existing data, the existing
                     data is replaced. If it overlaps a gap, the data is not replaced. Data past the
                     current end of channel is appended (with a gap if not contiguous).
        \return      S64_OK (0) or a negative error code.
        */
        virtual TSTime64 WriteData(const short* pData, size_t count, TSTime64 tFrom){ return CHANNEL_TYPE; }

        //! Write float waveform data to the channel
        /*!
        This is overridden by each channel type that implements writing of float waveforms.
        \param pData Pointer to the data to write.
        \param count The number of data values to write.
        \param tFrom The time of the first data value. If this overlaps existing data, the existing
        data is replaced. If it overlaps a gap, the data is not replaced. Data past the
        current end of channel is appended (with a gap if not contiguous).
        \return      S64_OK (0) or a negative error code.
        */
        virtual TSTime64 WriteData(const float* pData, size_t count, TSTime64 tFrom){ return CHANNEL_TYPE; }

        //! Edit a marker at a given time
        /*!
        This is overridden by channel classes that can handle it. It is an error to use it in the
        base class.

        Find a marker that exactly matches a given time. Then if nCopy exceeds sizeof(TSTime64)
        we adjust the marker and mark the block as modified.
        \param t    The time to find in the buffer
        \param pM   The marker holding replacement information (ignore the time)
        \param nCopy The number of bytes of marker info to change (but we do not change the time).
        */
        virtual int EditMarker(TSTime64 t, const TMarker* pM, size_t nCopy){ return CHANNEL_TYPE; }

        //! Get the size of any circular write buffer implemented for the channel
        /*!
        This is overridden by channel classes that implement a circular write buffer.
        \return The size of the write buffer in data items or 0 if no buffer
        */
        virtual size_t WriteBufferSize() const {return 0;}

        //=================================================================================
        // Routines to read data that are overridden in classes that implement them.

        //! Read EventBoth data as TSTime64 from level channels
        /*!
        This is overridden by channels that can handle the call. The base class returns an error.
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
        virtual int ReadLevelData(TSTime64* pData, CSRange& r, bool &bLevel){ return CHANNEL_TYPE; }

        virtual int ReadData(TSTime64* pData, CSRange& r, const CSFilter* pFilter = nullptr);
        virtual int ReadData(TMarker* pData, CSRange& r, const CSFilter* pFilter = nullptr);

        //! Read extended marker data
        /*!
        The base class is overridden by chanels that can handle this call. The base class returns an error.
        \param pData    Points at a buffer that can hold at least r.Max() items of read data.
        \param r        Defines the time range and count of items to read.
        \param pFilter  An optional filter to limit the codes of the returned values.
        \return         The number of items read or a negative error code.
        */
        virtual int ReadData(TExtMark* pData, CSRange& r, const CSFilter* pFilter = nullptr){return CHANNEL_TYPE;}

        //! Read waveform data as short
        /*!
        The base class is overridden by chanels that can handle this call. The base class returns an error.
        \param pData    Points at a buffer that can hold at least r.Max() items.
        \param r        Defines the time range and count of items to read.
        \param tFirst   Set to the time of the first read point, unchanged if no points or error.
        \param pFilter  Optional filter used when reading from extended marker channels.
        \return         The number of read items or a negative error code.
        */
        virtual int ReadData(short* pData, CSRange& r, TSTime64& tFirst, const CSFilter* pFilter = nullptr){return CHANNEL_TYPE;}

        //! Read waveform data as float
        /*!
        The base class is overridden by chanels that can handle this call. The base class translates
        a read as shorts into a read of floats.
        \param pData    Points at a buffer that can hold at least r.Max() items.
        \param r        Defines the time range and count of items to read.
        \param tFirst   Set to the time of the first read point, unchanged if no points or error.
        \param pFilter  Optional filter used when reading from extended marker channels.
        \return         The number of read items or a negative error code.
        */
        virtual int ReadData(float* pData, CSRange& r, TSTime64& tFirst, const CSFilter* pFilter = nullptr);

        //=============================================================================================
        // Routines to handle save/no save of data in circular buffers. They do nowt in the base class

        //! Set save/nosave state for the channel
        /*!
        This call erases all previously set state changes for time after time t. The default state
        when a channel is created is to save from time 0.
        \param t    The start time of the save state set by bSave. This state runs to the end of time.
        \param bSave If true, mark all data from time t onwards to be saved. If false, mark it to be
                     unsaved
        */
        virtual void Save(TSTime64 t, bool bSave){}

        //! Add a time range for saving to the list of times to save
        /*!
        To use this you set the background saving state to false with Save(t, false), then
        make calls to this routine to mark regions you want to keep.
        \param tFrom The start of the time range to save.
        \param tUpto The non-inclusive end of the range to save.
        */
        virtual void SaveRange(TSTime64 tFrom, TSTime64 tUpto){}

        //! Report the save state at a given time
        /*!
        If you use this with a non-buffered channel the result is always true. If you
        ask about a channel that does not exist, the result is always false. We can only
        say false if the data is in the circular buffer and is marked for non-saving. Once
        data is too old to be in the circular buffer we assume it is saved as if it was not,
        it will fail to be read from disk.
        \param tAt  The time at which we want to know about saving
        \return true if data written to the channel at the nominated time is currently
                     scheduled to be saved (or is in the circular buffer and has been saved).
        */
        virtual bool IsSaving(TSTime64 tAt) const {return true;}

        //! Get a list of times where saving is turned off and on
        /*!
        This returns the list of times held against the circular buffer which determine what
        data is saved when it is ejected from the circular buffer system. The list is kept for
        all the data in the circular buffer even if it has been committed to disk. This is
        only a snapshot of the state; it is used when drawing to indicate which data is being
        saved and should only be used as a guide.
        \param pTimes Points at an array of TSTime64 to be filled in with times, the first
        time is always for saveing being turned off. The next if for on, and so
        on. If the last item is an off time, saving is off to tUpto.
        \param nMax   The maximum number of transitions to return. If this is 0, return the
        number of changes we would return if there was no limit.
        \param tFrom  The start time from which we would like information.
        \param tUpto  The time up to but not including which we would like information.
        \return       The number of items that were added to the list. 0 means always saving. If
                      nMax is 0 then return the number of items we would return, given a chance.
                      Beware async additions.
        */
        virtual int  NoSaveList(TSTime64* pTimes, int nMax, TSTime64 tFrom = -1, TSTime64 tUpto = TSTIME64_MAX) const { return 0; }

        //! Tell circular buffering the latest time we have reached
        /*!
        Tell the channel the latest time for which all data has been delivered. This gives
        the channel the opportunity to clean up the save/nosave buffer.
        \param t    All data for this channel up to this time is complete, there will be NO more data
                    written to this channel before this time.
        */
        virtual void LatestTime(TSTime64 t){}

        //! Resize or create the circular buffer
        /*!
        This is overridden by channels with circular buffers implemented. It has no effect in the
        base class. Resizing while sampling using the buffer will work, but will very likely lose
        data. Setting a size of 0 stops circular buffering and trashes any buffered data.
        \param nItems The new size of the buffer in data items
        */
        virtual void ResizeCircular(size_t nItems){}

    protected:
        virtual int InitWriteBlock(CDataBlock* pDB);

        //! Change data already written to the channel
        /*!
        Waveform write calls are broken down into calls that overwite existing data and calls that
        write new data. This handles the changing existing data.
        \param pData The waveform to use to modify the channel.
        \param count The number of data points.
        \param tFrom The time of the first data point.
        */
        virtual int ChangeData(const short* pData, size_t count, TSTime64 tFrom){return CHANNEL_TYPE;}

        //! Change data already written to the channel
        /*!
        Waveform write calls are broken down into calls that overwite existing data and calls that
        write new data. This handles the changing existing data.
        \param pData The waveform to use to modify the channel.
        \param count The number of data points.
        \param tFrom The time of the first data point.
        */
        virtual int ChangeData(const float* pData, size_t count, TSTime64 tFrom){ return CHANNEL_TYPE; }

        virtual TSTime64 WriteBufferStartTime(TSTime64 tUpTo) const;
        virtual TSTime64 LastCommittedWriteTime() const;
        virtual TSTime64 MaxTimeNoLock() const;
        virtual void short2float(float* pf, const short* ps, size_t n) const;
        virtual void float2short(short* ps, const float* pf, size_t n) const;
        static bool TestNullFilter(const CSFilter*& pFilter);
        bool GetInitLevel() const;

        //! Get the last level written to the channel
        /*!
        Valid for EventBoth channels only. You _must_ hold the channel mutex to call this.
        Get the last level written to the channel. This is only used when we want to write
        data to the channel, and when we do this we will have allocated m_pWr. If there is
        no m_pWr, then just report the initial channel level.
        \return true for high, false for low (or if not a suitable channel).
        */
        virtual bool LastWriteLevel() const { return false; }

    private:
		virtual int LoadAppendList(bool bForReuse);
		virtual int IncAppendForReuse(unsigned int i = 0);
        virtual TDiskOff GetReuseOffsetSetTime(TSTime64 t);
        virtual int AddIndexItem(TDiskOff doItem, TSTime64 time, unsigned int level);
        virtual int SaveAppendIndex(int level);
        CSon64Chan(const CSon64Chan&);  // = delete; NO copy constructor 
        CSon64Chan& operator=(const CSon64Chan&); // = delete; No operator =
		unsigned int DepthFor();
    };

    //! Handles simple event channels without circular buffering
    /*!
    \internal
    Note that the basic read functionality is handled by the base class.
    */
    class CEventChan : public CSon64Chan
    {
    public:
        CEventChan(TSon64File& file, TChanNum nChan, TDataKind evtKind);
        virtual int WriteData(const TSTime64* pData, size_t count);
    };

    //! Simple event channels with circular write buffer
    /*!
    \internal
    */
    class CBEventChan : public CEventChan
    {
        typedef CircBuffer<TSTime64> circ_buff; //!< The circular buffer type
        unique_ptr<circ_buff> m_pCirc;          //!< The circular buffer used during writing
        size_t m_nMinMove;                      //!< Minimum items to move to disk
        mutable std::mutex m_mutBuf;            //!< buffer mutex (MUST acquire before mutHead)
        typedef std::lock_guard<std::mutex> TBufLock; //!< type used to acquire mutex

    protected:
        int CommitToWriteBuffer(TSTime64 tUpTo = TSTIME64_MAX);

    public:
        CBEventChan(TSon64File& file, TChanNum nChan, TDataKind evtKind, size_t bSize = DBSize / sizeof(TSTime64));
        virtual int WriteData(const TSTime64* pData, size_t count);
        virtual int ReadData(TSTime64* pData, CSRange& r, const CSFilter* pFilter = nullptr);
        virtual TSTime64 MaxTime() const;
        virtual TSTime64 PrevNTime(CSRange& r, const CSFilter* pFilter = nullptr, bool bAsWave = false);

        virtual void ResizeCircular(size_t nItems);
        virtual size_t WriteBufferSize() const {return m_pCirc ? m_pCirc->size() : 0;}
        virtual uint64_t GetChanBytes() const;

        virtual int Commit();
        virtual bool IsModified() const;         //! True if anything is commitable
        virtual int EmptyForReuse()
        {
            if (m_pCirc)
                m_pCirc->flush();
            return CEventChan::EmptyForReuse();
        }

        virtual void Save(TSTime64 t, bool bSave);
        virtual void SaveRange(TSTime64 tFrom, TSTime64 tUpto);
        virtual bool IsSaving(TSTime64 tAt) const;
        virtual int  NoSaveList(TSTime64* pTimes, int nMax, TSTime64 tFrom = -1, TSTime64 tUpto = TSTIME64_MAX) const;
        virtual void LatestTime(TSTime64 t);
    };

    //! class to handle existing marker channels (and level channels)
    /*!
    \internal
    */
    class CMarkerChan : public CSon64Chan
    {
        int WriteDataLocked(const TMarker* pData, size_t count);
    protected:
        bool LastWriteLevel() const;

    public:
        CMarkerChan(TSon64File& file, TChanNum nChan, TDataKind kind = Marker);
        virtual int WriteData(const TMarker* pData, size_t count);
        virtual int WriteData(const TSTime64* pData, size_t count);
        virtual int ReadLevelData(TSTime64* pData, CSRange& r, bool &bLevel);    // special read
        virtual int EditMarker(TSTime64 t, const TMarker* pM, size_t nCopy);
    };

	//! class to handle buffered marker (and level) channels
    /*!
    \internal
    */
    class CBMarkerChan : public CMarkerChan
    {
        int WriteDataLocked(const TMarker* pData, size_t count);
        bool LastWriteLevel() const;
    protected:
        typedef CircBuffer<TMarker> circ_buff;  //!< typedef to save typing
        unique_ptr<circ_buff> m_pCirc;          //!< Object to hold any created circular buffer
        size_t m_nMinMove;                      //!< Minimum wave points to move when full
        mutable std::mutex m_mutBuf;            //!< The buffer mutex (MUST acquire before mutHead)
        typedef std::lock_guard<std::mutex> TBufLock;   //!< type used to lock the mutes
        int CommitToWriteBuffer(TSTime64 tUpTo = TSTIME64_MAX);

    public:
        CBMarkerChan(TSon64File& file, TChanNum nChan, TDataKind kind = Marker, size_t bSize = DBSize / sizeof(TMarker));
        virtual int WriteData(const TMarker* pData, size_t count);
        virtual int ReadData(TSTime64* pData, CSRange& r, const CSFilter* pFilter = nullptr);
        virtual int ReadData(TMarker* pData, CSRange& r, const CSFilter* pFilter = nullptr);
        virtual TSTime64 MaxTime() const;
        virtual TSTime64 PrevNTime(CSRange& r, const CSFilter* pFilter = nullptr, bool bAsWave = false);
        virtual void ResizeCircular(size_t nItems);
        virtual size_t WriteBufferSize() const {return m_pCirc ? m_pCirc->size() : 0;}
        virtual uint64_t GetChanBytes() const;
        virtual int WriteData(const TSTime64* pData, size_t count);
        virtual int EditMarker(TSTime64 t, const TMarker* pM, size_t nCopy);

        virtual int Commit();
        virtual bool IsModified() const;
        virtual int EmptyForReuse()
        {
            if (m_pCirc)
                m_pCirc->flush();
            return CMarkerChan::EmptyForReuse();
        }

        virtual void Save(TSTime64 t, bool bSave);
        virtual void SaveRange(TSTime64 tFrom, TSTime64 tUpto);
        virtual bool IsSaving(TSTime64 tAt) const;
        virtual int  NoSaveList(TSTime64* pTimes, int nMax, TSTime64 tFrom = -1, TSTime64 tUpto = TSTIME64_MAX) const;
        virtual void LatestTime(TSTime64 t);
    };

	//! Class to handle extended marker channels
    /*!
    \internal
    */
    class CExtMarkChan : public CSon64Chan
    {
    public:
        CExtMarkChan(TSon64File& file, TChanNum nChan, TDataKind xKind, size_t nRow, size_t nCol = 1, TSTime64 tDvd = 0);
        virtual int WriteData(const TExtMark* pData, size_t count);
        virtual int ReadData(short* pData, CSRange& r, TSTime64& tFirst, const CSFilter* pFilter = nullptr);
        virtual int ReadData(TExtMark* pData, CSRange& r, const CSFilter* pFilter = nullptr);
        virtual int EditMarker(TSTime64 t, const TMarker* pM, size_t nCopy);
    };

	//! Class to handle buffered extended marker channels
    /*!
    \internal
    */
    class CBExtMarkChan : public CExtMarkChan
    {
        typedef CircBuffer<TExtMark> circ_buff;
        unique_ptr<circ_buff> m_pCirc;
        size_t m_nMinMove;
        mutable std::mutex m_mutBuf;    // buffer mutex (MUST acquire before mutHead)
        typedef std::lock_guard<std::mutex> TBufLock;

    protected:
        int CommitToWriteBuffer(TSTime64 tUpTo = TSTIME64_MAX);

    public:
        CBExtMarkChan(TSon64File& file, TChanNum nChan, size_t bSize, TDataKind xKind, size_t nRow, size_t nCol = 1, TSTime64 tDvd=0);
        virtual int WriteData(const TExtMark* pData, size_t count);
        virtual int ReadData(short* pData, CSRange& r, TSTime64& tFirst, const CSFilter* pFilter = nullptr);
        virtual int ReadData(TSTime64* pData, CSRange& r, const CSFilter* pFilter = nullptr);
        virtual int ReadData(TMarker* pData, CSRange& r, const CSFilter* pFilter = nullptr);
        virtual int ReadData(TExtMark* pData, CSRange& r, const CSFilter* pFilter = nullptr);
        virtual TSTime64 MaxTime() const;
        virtual TSTime64 PrevNTime(CSRange& r, const CSFilter* pFilter = nullptr, bool bAsWave = false);
        virtual void ResizeCircular(size_t nItems);
        virtual size_t WriteBufferSize() const {return m_pCirc ? m_pCirc->size() : 0;}
        virtual uint64_t GetChanBytes() const;
        virtual int EditMarker(TSTime64 t, const TMarker* pM, size_t nCopy);

        virtual int Commit();
        virtual bool IsModified() const;
        virtual int EmptyForReuse()
        {
            if (m_pCirc)
                m_pCirc->flush();
            return CExtMarkChan::EmptyForReuse();
        }

        virtual void Save(TSTime64 t, bool bSave);
        virtual void SaveRange(TSTime64 tFrom, TSTime64 tUpto);
        virtual bool IsSaving(TSTime64 tAt) const;
        virtual int  NoSaveList(TSTime64* pTimes, int nMax, TSTime64 tFrom = -1, TSTime64 tUpto = TSTIME64_MAX) const;
        virtual void LatestTime(TSTime64 t);
    };

	//! Class to handle 16-bit integer waveform channels
    /*!
    \internal
    */
    class CAdcChan : public CSon64Chan
    {
    public:
        CAdcChan(TSon64File& file, TChanNum nChan, TSTime64 tDivide);
        virtual TSTime64 WriteData(const short* pData, size_t count, TSTime64 tFrom);
        virtual int ChangeData(const short* pData, size_t count, TSTime64 tFrom);
        virtual int ReadData(short* pData, CSRange& r, TSTime64& tFirst, const CSFilter* pFilter = nullptr);
    };

	//! Class to handle buffered 16-bit integer waveform channels
    /*!
    \internal
    */
    class CBAdcChan : public CAdcChan
    {
        typedef CircWBuffer<short> circ_buff;
        unique_ptr<circ_buff> m_pCirc;
        size_t m_nMinMove;
        mutable std::mutex m_mutBuf;    // buffer mutex
        typedef std::lock_guard<std::mutex> TBufLock;

    protected:
        int CommitToWriteBuffer(TSTime64 tUpTo = TSTIME64_MAX);

    public:
        CBAdcChan(TSon64File& file, TChanNum nChan, TSTime64 tDivide, size_t bSize = DBSize/sizeof(short));
        virtual TSTime64 WriteData(const short* pData, size_t count, TSTime64 tFrom);
        virtual int ChangeData(const short* pData, size_t count, TSTime64 tFrom);
        virtual int ReadData(short* pData, CSRange& r, TSTime64& tFirst, const CSFilter* pFilter = nullptr);
        virtual TSTime64 MaxTime() const;
        virtual TSTime64 PrevNTime(CSRange& r, const CSFilter* pFilter = nullptr, bool bAsWave = false);
        virtual void ResizeCircular(size_t nItems);
        virtual size_t WriteBufferSize() const {return m_pCirc ? m_pCirc->size() : 0;}
        virtual uint64_t GetChanBytes() const;

        virtual int Commit();
        virtual bool IsModified() const;
        virtual int EmptyForReuse()
        {
            if (m_pCirc)
                m_pCirc->flush();
            return CAdcChan::EmptyForReuse();
        }

        virtual void Save(TSTime64 t, bool bSave);
        virtual void SaveRange(TSTime64 tFrom, TSTime64 tUpto);
        virtual bool IsSaving(TSTime64 tAt) const;
        virtual int  NoSaveList(TSTime64* pTimes, int nMax, TSTime64 tFrom = -1, TSTime64 tUpto = TSTIME64_MAX) const;
        virtual void LatestTime(TSTime64 t);
    };

	//! Class to handle float waveform channels
    /*!
    \internal
    */
    class CRealWChan : public CSon64Chan
    {
    public:
        CRealWChan(TSon64File& file, TChanNum nChan, TSTime64 tDivide);
        virtual TSTime64 WriteData(const float* pData, size_t count, TSTime64 tFrom);
        virtual int ChangeData(const float* pData, size_t count, TSTime64 tFrom);
        virtual int ReadData(short* pData, CSRange& r, TSTime64& tFirst, const CSFilter* pFilter = nullptr);
        virtual int ReadData(float* pData, CSRange& r, TSTime64& tFirst, const CSFilter* pFilter = nullptr);
    };

	//! Class to handle buffered float waveform channels
    /*!
    \internal
    */
    class CBRealWChan : public CRealWChan
    {
        typedef CircWBuffer<float> circ_buff;
        unique_ptr<circ_buff> m_pCirc;
        size_t m_nMinMove;
        mutable std::mutex m_mutBuf;    // buffer mutex
        typedef std::lock_guard<std::mutex> TBufLock;

    protected:
        int CommitToWriteBuffer(TSTime64 tUpTo = TSTIME64_MAX);

    public:
        CBRealWChan(TSon64File& file, TChanNum nChan, TSTime64 tDivide, size_t bSize = DBSize/sizeof(short));
        virtual TSTime64 WriteData(const float* pData, size_t count, TSTime64 tFrom);
        virtual int ChangeData(const float* pData, size_t count, TSTime64 tFrom);
        virtual int ReadData(float* pData, CSRange& r, TSTime64& tFirst, const CSFilter* pFilter = nullptr);
        virtual TSTime64 MaxTime() const;
        virtual TSTime64 PrevNTime(CSRange& r, const CSFilter* pFilter = nullptr, bool bAsWave = false);
        virtual void ResizeCircular(size_t nItems);
        virtual size_t WriteBufferSize() const {return m_pCirc ? m_pCirc->size() : 0;}
        virtual uint64_t GetChanBytes() const;

        virtual int Commit();
        virtual bool IsModified() const;
        virtual int EmptyForReuse()
        {
            if (m_pCirc)
                m_pCirc->flush();
            return CRealWChan::EmptyForReuse();
        }

        virtual void Save(TSTime64 t, bool bSave);
        virtual void SaveRange(TSTime64 tFrom, TSTime64 tUpto);
        virtual bool IsSaving(TSTime64 tAt) const;
        virtual int  NoSaveList(TSTime64* pTimes, int nMax, TSTime64 tFrom = -1, TSTime64 tUpto = TSTIME64_MAX) const;
        virtual void LatestTime(TSTime64 t);
   };
}
#undef DllClass
#endif