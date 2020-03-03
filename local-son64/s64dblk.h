//s64dblk.h
#ifndef __S64DBLK_H__
#define __S64DBLK_H__
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

//! \file s64dblk.h
//! \brief Data block definitions for the SON64 file
/*!
\internal
This file defines the various types of data block that make up a SON64 file. All
data blocks have a TDiskBlockHead, matching the directory blocks.
*/

#include "s64priv.h"
#include "s64filt.h"
#include "s64iter.h"
#include "s64witer.h"

using namespace ceds64;
namespace ceds64
{
    class CSRange;

	//! This is used to handle sub-blocks of waveform data in a data block
	/*!
	 * Waveform data is saved as blocks of contiguous data, each of which starts
	 * with a header that includes the number of following data points of type T.
	 * Each TWave<T> starts on an 8-byte boundary on disk and in memory to ensure
	 * no problems with data alignment on any known machine.
	 *
	 * This type starts with the same header as an event-based channel so that code
	 * can be shared to get the first time in a block.
	 */
    template<typename T>
    struct TWave
    {
        TSTime64    m_startTime;		//!< Time of the first data item
        uint32_t    m_nItems;           //!< number of items that follow
        uint32_t    m_pad;              //!< pad to 16 bytes (set 0, reserved)
        T           m_data[(DBSize-DBHSize-16)/sizeof(T)];  //!< data starts here
        enum {TWAVE_HEADSIZE = sizeof(TSTime64) + 2*sizeof(uint32_t)};
    };

    enum
    {
        MAX_EVENT = (DBSize - DBHSize) / sizeof(TSTime64),
        MAX_MARK = (DBSize - DBHSize) / sizeof(TMarker),
        MAX_ADC = (DBSize-DBHSize-16) / sizeof(short),
        MAX_REALWAVE = (DBSize-DBHSize-16) / sizeof(float),
    };

	//! The data block as stored on disk
	/*!
	 * TDataBlock describes the memory structure that is stored on disk.
	 *
	 * All blocks stored on disk start with a TDiskBlockHead that says how many items
	 * are in this block, followed by the items. Note that for waveform channels, the
	 * items are not all the same size as they are for event-based channels.
	 * In all cases, the first data item after the header is the time of the first item
	 * in the block.
	 */
    struct TDataBlock : public TDiskBlockHead
    {
        union
        {
            TSTime64 m_event[MAX_EVENT];    //!< Events, set to fill the entire block
            TMarker  m_mark[MAX_MARK];      //!< Markers, set to fill the entre block
            TExtMark m_eMark[1];            //!< Start of first item; size known at run time
            TWave<short> m_adc;				//!< Start of first item; varying size
            TWave<float> m_realwave;		//!< Start of first item; varying size
        };
    };

	//! Wrapper for memory copy of TDataBlock to track disk position and if saved
    /*!
    \internal
    */
    class CDataBlock : public TDataBlock
    {
        TDiskOff    m_do;               //!< where it goes on disk or 0 if not written
        bool        m_bUnsaved;         //!< true if there is unsaved data (needs writing)
    public:
        CDataBlock() :  m_do( 0 ), m_bUnsaved( false ) {}

        /*!
        Construct a data block for a channel
        \param chan The channel number.
        */
        explicit CDataBlock(TChanNum chan)
            : m_do( 0 )
            , m_bUnsaved( false )
        {
            Init(0, chan);
        }

        TDataBlock* DataBlock() {return this;}              //!< Get the TDataBlock
        const TDataBlock* DataBlock() const {return this;}  //!< Get a const TDataBlock

        uint32_t size() const {return m_nItems;}            //!< Get items in the block
        bool empty() const {return m_nItems == 0;}          //!< Test if block is empty
        void clear(){m_do = 0; m_nItems = 0; m_bUnsaved=false;} //!< Set the block empty
        virtual size_t max_size() const = 0;                //!< Get max item count for the block

        /*!
        Set the block position on disk
        \param pos The disk position to set.
        */
        void SetDiskOff(TDiskOff pos){m_do = pos;}
        TDiskOff DiskOff() const {return m_do;}             //!< Get the block disk position
        bool Unsaved() const {return m_bUnsaved && (size() > 0);}   //!< Test if needs saving
        void SetUnsaved(){m_bUnsaved = true;}               //!< Set the block unsaved
        void SetSaved(){m_bUnsaved = false;}                //!< Clear the unsaved flag
 
        //! Get the first time saved in the block or -1 if none
        /*!
        FirstTime works because all blocks have the time as the first payload item
        \return Time of the first item or -1 if the block is empty.
        */
        TSTime64 FirstTime() const {return m_nItems ? m_event[0] : -1;}

        // Routines that are used in a generic way for all data blocks
        virtual TSTime64 LastTime() const = 0;              //!< Time of the last item in a block or -1 if none.

        //! Get the last code written to the data block
        /*!
        LastCode is used with EventBoth data to predict the next code to use.
        \return The last code written to an EventBoth channel or 0 for all others.
        */
        virtual int LastCode() const {return 0;}

        //! Read event data from the data block
        /*!
        Copy events from the current read buffer in a given time range and indicate if we found
        data past the time range or we hit the maximum number.
        \param pData    The target buffer with space for at least r.Max() items. This is updated.
        \param r        The range to fetch, including the max number to return. This is adjusted to show
                        done if we find data at or beyond the Upto time.
        \param pFilter  Used with Marker and extended marker channels to filter the data.
        \return         The number of items copied from the buffer or CHANNEL_TYPE error if not possible.
        */
        virtual int GetData(TSTime64*& pData, CSRange& r, const CSFilter* pFilter = nullptr) const {return CHANNEL_TYPE;}

        //! Read Marker data from the data block
        /*!
        Copy events from the current read buffer in a given time range and indicate if we found
        data past the time range or we hit the maximum number.
        \param pData    The target buffer with space for at least r.Max() items. This is updated.
        \param r        The range to fetch, including the max number to return. This is adjusted to show
                        done if we find data at or beyond the Upto time.
        \param pFilter  Used to filter the data.
        \return         The number of items copied from the buffer or CHANNEL_TYPE error if not possible.
        */
        virtual int GetData(TMarker*& pData, CSRange& r, const CSFilter* pFilter = nullptr) const { return CHANNEL_TYPE; }

        //! Read extended marker data from the data block
        /*!
        Copy events from the current read buffer in a given time range and indicate if we found
        data past the time range or we hit the maximum number.
        \param pData    The target buffer with space for at least r.Max() items. This is updated.
        \param r        The range to fetch, including the max number to return. This is adjusted to show
                        done if we find data at or beyond the Upto time.
        \param pFilter  Used to filter the data.
        \return         The number of items copied from the buffer or CHANNEL_TYPE error if not possible.
        */
        virtual int GetData(TExtMark*& pData, CSRange& r, const CSFilter* pFilter = nullptr) const { return CHANNEL_TYPE; }

        //! Read wavefrom data from the data block
        /*!
        Read data into a pointer and move the pointer onwards. If r.m_bFirst is true, we will accept
        the first data point at or after r.m_tFrom. If it is false then the first data point MUST
        be at r.m_tFrom, otherwise we must return 0 points.
        \param pData    Points at the buffer to be filled. Must have space for r.Max() items.
        \param r        The time range. If no more data is possible, call r.Done(true). If more is
                        possible update m_tFrom to the next contiguous time. m_bFirst determines if
                        we must match the start time or not. This also holds a trave number for use
                        when reading from AdcMark channels with multiple traces.
        \param tFirst   Returned holding the time of the first point if m_bFirst is true and we get
                        data.
        \param pFilter  Used when readng Adc from Adcmark channels.
        \return         The number of contiguous data points, or 0 if none or not contig, or negative
                        error, in which case the range is adjusted to say we are done.
        */
        virtual int GetData(short*& pData, CSRange& r, TSTime64& tFirst, const CSFilter* pFilter = nullptr) const{ return CHANNEL_TYPE; };

        //! Read wavefrom data from the data block
        /*!
        Read data into a pointer and move the pointer onwards. If r.m_bFirst is true, we will accept
        the first data point at or after r.m_tFrom. If it is false then the first data point MUST
        be at r.m_tFrom, otherwise we must return 0 points.
        \param pData    Points at the buffer to be filled. Must have space for r.Max() items.
        \param r        The time range. If no more data is possible, call r.Done(true). If more is
                        possible update m_tFrom to the next contiguous time. m_bFirst determines if
                        we must match the start time or not.
        \param tFirst   Returned holding the time of the first point if m_bFirst is true and we get
                        data.
        \param pFilter  Used when readng Adc from Adcmark channels.
        \return         The number of contiguous data points, or 0 if none or not contig, or negative
                        error, in which case the range is adjusted to say we are done.
        */
        virtual int GetData(float*& pData, CSRange& r, TSTime64& tFirst, const CSFilter* pFilter = nullptr) const{ return CHANNEL_TYPE; };

        //! Add waveform data into the buffer
        /*!
        If the buffer is already full we set it empty and then copy the data, otherwise we copy as
        much data as we can fit. If the new data is not contiguous with any existing data we add
        a gap into the buffer.
        \param pData    Reference to a pointer to the data to add. We add as much data as we can
                        then move the pointer onwards by this ammount.
        \param count    The number of data items to add (can be larger than the buffer size).
        \param tFrom    The time of the first waveform item.
        \return         The number of items added.
        */
        virtual int AddData(const short*& pData, size_t count, TSTime64 tFrom){return CHANNEL_TYPE;};

        //! Add waveform data into the buffer
        /*!
        If the buffer is already full we set it empty and then copy the data, otherwise we copy as
        much data as we can fit. If the new data is not contiguous with any existing data we add
        a gap into the buffer.
        \param pData    Reference to a pointer to the data to add. We add as much data as we can
                        then move the pointer onwards by this ammount.
        \param count    The number of data items to add (can be larger than the buffer size).
        \param tFrom    The time of the first waveform item.
        \return         The number of items added.
        */
        virtual int AddData(const float*& pData, size_t count, TSTime64 tFrom){ return CHANNEL_TYPE; };

        //! Add TSTime64 (event) data into the buffer
        /*!
        If the buffer is already full we set it empty and then copy the data, otherwise we copy as
        much data as we can fit.
        \param pData    Reference to a pointer to the data to add. We add as much data as we can
                        then move the pointer onwards by this ammount.
        \param count    The number of data items to add (can be larger than the buffer size).
        \return         The number of items added.
        */
        virtual int AddData(const TSTime64*& pData, size_t count) { return CHANNEL_TYPE; }

        //! Add TMarker data into the buffer
        /*!
        If the buffer is already full we set it empty and then copy the data, otherwise we copy as
        much data as we can fit.
        \param pData    Reference to a pointer to the data to add. We add as much data as we can
                        then move the pointer onwards by this ammount.
        \param count    The number of data items to add (can be larger than the buffer size).
        \return         The number of items added.
        */
        virtual int AddData(const TMarker*& pData, size_t count) { return CHANNEL_TYPE; }

        //! Add extended marker data into the buffer
        /*!
        If the buffer is already full we set it empty and then copy the data, otherwise we copy as
        much data as we can fit.
        \param pData    Reference to a pointer to the data to add. We add as much data as we can
        then move the pointer onwards by this ammount.
        \param count    The number of data items to add (can be larger than the buffer size).
        \return         The number of items added.
        */
        virtual int AddData(const TExtMark*& pData, size_t count) { return CHANNEL_TYPE; }

        //! Change existing waveform data in the block
        /*!
        If the buffer passed in overlaps any data in the block, replace the matched data. If the
        data does not align exactly, we round the position down (maybe should go to nearer?). The
        block is set unsaved if any data is changed.
        \param pData    Pointer to the replacement data.
        \param count    The number of points of replacement data available.
        \param tFrom    Time of the first data item.
        \param first    If the return value is >0, this is set to the index in pData of the first
                        data point that is used.
        \return         -1 if no overlap and data ends before the buffer, 0 the buffer starts past
                        the block, else the number of points that overlap the block (even if they
                        all lie in a gap, so no change is made).
        */
        virtual int ChangeWave(const short* pData, size_t count, TSTime64 tFrom, size_t& first) { return CHANNEL_TYPE; }

        //! Change existing waveform data in the block
        /*!
        If the buffer passed in overlaps any data in the block, replace the matched data. If the
        data does not align exactly, we round the position down (maybe should go to nearer?). The
        block is set unsaved if any data is changed.
        \param pData    Pointer to the replacement data.
        \param count    The number of points of replacement data available.
        \param tFrom    Time of the first data item.
        \param first    If the return value is >0, this is set to the index in pData of the first
                        data point that is used.
        \return         -1 if no overlap and data ends before the buffer, 0 the buffer starts past
        the block, else the number of points that overlap the block (even if they
        all lie in a gap, so no change is made).
        */
        virtual int ChangeWave(const float* pData, size_t count, TSTime64 tFrom, size_t& first) { return CHANNEL_TYPE; }

        //! Find the r.Max() point before a given time
        /*!
        \param r        This defines the time range for the search and the number of items we
                        want to go back.
        \param pFilt    If not nullptr (and the block is a marker or an extended marker type), use
                        this to filter the data. That is, items not in the filter are ignored.
        \return         -1 if not found (but r.Max() is reduced by the number of points skipped and
                        the range is adjusted to replace the new end (r.tUpto) for the next search
                        in the previous block). Otherwise, the found time and r.Max() is set 0.
        */
        virtual TSTime64 PrevNTime(CSRange& r, const CSFilter* pFilt = nullptr) const = 0;

        //! Search an extended marker block for waveform data in a range and count backwards through it
        /*!
        Search backwards in the range defined by r. If this is the first call, find the last data in the
        range, otherwise data must occur exactly lDvd before r.Upto(), else we do not match.
        \param r        Defines the time range to search and if this is the first search or not.
        \param pFilt    Defines a filter to apply to the the extended marker blocks
        \param nRow     The number of logical items starting at each item time
        \param tDvd     The sample interval in clock ticks between the items
        */
        virtual TSTime64 PrevNTimeW(CSRange& r, const CSFilter* pFilt, size_t nRow, TSTime64 tDvd) const { return CHANNEL_TYPE; }

        //! Edit a marker at a given time
        /*!
        Find a marker that exactly matches a given time. Then if nCopy exceeds sizeof(TSTime64)
        we adjust the marker and mark the block as modified.
        \param t    The time to find in the buffer
        \param pM   The marker holding replacement information (ignore the time)
        \param nCopy The number of bytes of marker info to change (but we do not change the time).
        */
        virtual int EditMarker(TSTime64 t, const TMarker* pM, size_t nCopy) { return CHANNEL_TYPE; }

        //! The data block has been modified, clear any cached data
        /*!
        We have updated the data block contents. Some derived classes hold cached values (a
        pointer to the last used item), so this needs invalidating.
        */
        virtual void NewDataRead(){};   // Opportunity to clean any cached data

    protected:
        CSFilter::eActive TestActive(CSRange& r, const CSFilter* pFilt = nullptr) const;
    };

	//! Handles blocks of event data
    /*!
    \internal
    */
    class CEventBlock : public CDataBlock
    {
        typedef db_iterator<TSTime64> iter;			//!< To iterate through the data
        typedef db_iterator<const TSTime64> citer;	//!< To iterate through the data
        const size_t m_itemSize;					//!< bytes to hold the item
    public:
        //! CEventBlock constructor
        /*!
        \param chan The channel number
        */
        explicit CEventBlock(TChanNum chan)
            : CDataBlock(chan)
            , m_itemSize(sizeof(TSTime64))
        {}

        virtual size_t max_size() const {return MAX_EVENT;}             //!< Get maxitems in the buffer
        size_t capacity() const {return MAX_EVENT;}                     //!< Get max items in the buffer
        bool full() const { return size() >= capacity(); }              //!< Is this buffer full?
        iter begin() {return iter(m_event, sizeof(TSTime64));}          //!< Get iterator to the start
        iter end() {return iter(m_event, sizeof(TSTime64))+m_nItems;}   //!< Get iterator to the end
        citer cbegin() const {return citer(m_event, sizeof(TSTime64));} //!< Get const iteretor to the start
        citer cend() const {return citer(m_event, sizeof(TSTime64))+m_nItems;}  //!< Get const iterator to the end
        citer IterFor(TSTime64 t) const;

        // Routines that are used in a generic way for all data blocks
        virtual TSTime64 LastTime() const {return m_nItems ? m_event[m_nItems-1] : -1;}
        virtual int AddData(const TSTime64*& pData, size_t count);
        virtual int GetData(TSTime64*& pData, CSRange& r, const CSFilter* pFilter = nullptr) const;
        virtual TSTime64 PrevNTime(CSRange& r, const CSFilter* pFilt = nullptr) const;
    };

	//! Handles blocks of marker data
    /*!
    \internal
    */
    class CMarkerBlock : public CDataBlock
    {
        typedef db_iterator<TMarker> iter;			//!< To iterate through the buffer data
        typedef db_iterator<const TMarker> citer;	//!< To iterate through data for reading
        const size_t m_itemSize;					//!< bytes to hold the item
    public:
        /*!
        Constuct a marker block for a particular channel
        \param chan The channel number.
        */
        explicit CMarkerBlock(TChanNum chan)
            : CDataBlock(chan)
            , m_itemSize(sizeof(TMarker))
        {}

        virtual size_t max_size() const {return MAX_MARK;}
        size_t capacity() const {return MAX_MARK;}          //!< Capacity of this block
        bool full() const { return size() >= capacity(); }  //!< Is this buffer full?
        iter begin() {return iter(m_mark, sizeof(TMarker));} //!< iterator to start
        iter end() {return iter(m_mark, sizeof(TMarker))+m_nItems;} //!< iterator to the end
        citer cbegin() const {return citer(m_mark, sizeof(TMarker));} //!< const iterator to the start
        citer cend() const {return citer(m_mark, sizeof(TMarker))+m_nItems;} //!< const end iterator
        citer IterFor(TSTime64 t) const;            //!< Get iterator for a particular time

        // LastCode is used with EventBoth data to predict the next code to use
        virtual int LastCode() const {return m_nItems ? m_mark[m_nItems-1].m_code[0] : -1;}

        // Routines that are used in a generic way for all data blocks
        virtual TSTime64 LastTime() const {return m_nItems ? m_mark[m_nItems-1].m_time : -1;}
        virtual int AddData(const TMarker*& pData, size_t count);
        virtual int GetData(TSTime64*& pData, CSRange& r, const CSFilter* pFilter = nullptr) const;
        virtual int GetData(TMarker*& pData, CSRange& r, const CSFilter* pFilter = nullptr) const; 
        virtual TSTime64 PrevNTime(CSRange& r, const CSFilter* pFilt = nullptr) const;
        virtual int EditMarker(TSTime64 t, const TMarker* pN, size_t nCopy);
    };

	//! Handles blocks of extended marker data
    /*!
    \internal
    */
    class CExtMarkBlock : public CDataBlock
    {
        typedef db_iterator<TExtMark> iter;			//!< To iterate through the data
        typedef db_iterator<const TExtMark> citer;	//!< To iterate through the data
        const size_t m_maxItem;						//!< Maximum items in the buffer
        const size_t m_itemSize;					//!< bytes to hold the item
    public:
        //! Construct an extended marker block
        /*!
        \param chan The channel number this block belongs to
        \param size The size of this item in bytes (at least sizeof(TExtMark))
        */
        CExtMarkBlock(TChanNum chan, size_t size)
            : CDataBlock(chan)
            , m_maxItem((DBSize-DBHSize) / size)
            , m_itemSize(size)
            {}

        virtual size_t max_size() const {return m_maxItem;}         //!< get maximum items in the buffer
        size_t capacity() const {return m_maxItem;}                 //!< Get maximum items in the buffer
        bool full() const { return size() >= capacity(); }          //!< Is this buffer full?
        iter begin() {return iter(m_eMark, m_itemSize);}            //!< Get iterator to the start
        iter end() {return iter(m_eMark, m_itemSize)+m_nItems;}     //!< Get iterator to the end
        citer cbegin() const {return citer(m_eMark, m_itemSize);}   //!< Get const interator to the start
        citer cend() const {return citer(m_eMark, m_itemSize)+m_nItems;}    //!< Get const iterator to the end
        citer IterFor(TSTime64 t) const;                            // Get circular iterator to a time

        // Routines that are used in a generic way for all data blocks
        virtual TSTime64 LastTime() const {return m_nItems ? (cend()-1)->m_time : -1;}
        virtual int AddData(const TExtMark*& pData, size_t count);
        virtual int GetData(short*& pData, CSRange& r, TSTime64& tFirst, const CSFilter* pFilter = nullptr) const;
        virtual int GetData(TSTime64*& pData, CSRange& r, const CSFilter* pFilter = nullptr) const;
        virtual int GetData(TMarker*& pData, CSRange& r, const CSFilter* pFilter = nullptr) const; 
        virtual int GetData(TExtMark*& pData, CSRange& r, const CSFilter* pFilter = nullptr) const; 
        virtual TSTime64 PrevNTime(CSRange& r, const CSFilter* pFilt = nullptr) const;
        virtual TSTime64 PrevNTimeW(CSRange& r, const CSFilter* pFilt, size_t nRow, TSTime64 tDvd) const;
        virtual int EditMarker(TSTime64 t, const TMarker* pN, size_t nCopy);
    };

	//! Handles blocks of 16-bit integer data
    /*!
    \internal
    */
    class CAdcBlock : public CDataBlock
    {
        typedef wv_iterator<TWave<short>, short> witer;        //!< To iterate through sub-blocks of contiguous data
        typedef wv_iterator<const TWave<short>, short> cwiter; //!< To iterate through sub-blocks of contiguous data
        mutable TWave<short>* m_pBack;  //!< points at last used item or nullptr
        const TSTime64 m_tDivide;       //!< channel divisor
    public:
        //! Construct a CAdcBlock
        /*!
        \param chan     The channel number that this block belongs to.
        \param tDivide  The divide down from the file time base
        */
        CAdcBlock(TChanNum chan, TSTime64 tDivide)
            : CDataBlock(chan)
            , m_pBack( nullptr )
            , m_tDivide( tDivide )
            {}

        virtual size_t max_size() const {return MAX_ADC;}
        witer begin() {return witer(&m_adc);}   //!< Get iterator to the first sub-block
        witer end();                            //!< Get iterator to past the last sub-block
        cwiter cbegin() const {return cwiter(&m_adc);}  //!< const version of begin()
        cwiter cend() const;                    //!< const version of end()
        const TWave<short>& back() const;       //!< const reference to last element (must exist).
        TWave<short>& back();                   //!< reference to final element, which must exist.

        size_t SpaceContiguous() const;         //!< Space in the block for contiguous data
        size_t SpaceNonContiguous() const;      //!< Space in the block for non-contiguous data

        // Routines that are used in a generic way for all data blocks
        virtual TSTime64 LastTime() const;
        virtual int AddData(const short*& pData, size_t count, TSTime64 tFrom);
        virtual int GetData(short*& pData, CSRange& r, TSTime64& tFirst, const CSFilter* pFilter = nullptr) const;
        virtual TSTime64 PrevNTime(CSRange& r, const CSFilter* pFilt = nullptr) const;
        virtual void NewDataRead(){m_pBack = nullptr;}   // Opportunity to clean any cached data

        // Routines for Wave blocks
        virtual int ChangeWave(const short* pData, size_t count, TSTime64 tFrom, size_t& first);
    };

	//! Handles blocks of float waveforms
    /*!
    \internal
    */
    class CRealWaveBlock : public CDataBlock
    {
        typedef wv_iterator<TWave<float>, float> witer; //!< To iterate through sub-blocks of contiguous data
        typedef wv_iterator<const TWave<float>, float> cwiter; //!< To iterate through sub-blocks of contiguous data
        mutable TWave<float>* m_pBack;  //!< points at last used item or nullptr
        const TSTime64 m_tDivide;       //!< channel divisor
    public:

        /*!
        Constructor
        \param chan The channel number for this block.
        \param tDivide The sample interval in file time units
        */
        CRealWaveBlock(TChanNum chan, TSTime64 tDivide)
            : CDataBlock(chan)
            , m_pBack( nullptr )
            , m_tDivide( tDivide )
        {}

        virtual size_t max_size() const {return MAX_REALWAVE;}
        witer begin() {return witer(&m_realwave);}      //!< Iterator to the first item
        witer end();                                    //!< Iterator past the end
        cwiter cbegin() const {return cwiter(&m_realwave);} //!< const iterator to the first item
        cwiter cend() const;                            //!< const iterator past the end
        const TWave<float>& back() const;               //!< const reference to last item
        TWave<float>& back();                           //!< reference to the last item

        size_t SpaceContiguous() const;                 //!< Maximum contiguous data we could add
        size_t SpaceNonContiguous() const;              //!< Maximum non-contiguous data we could add

        // Routines that are used in a generic way for all data blocks
        virtual TSTime64 LastTime() const;
        virtual int AddData(const float*& pData, size_t count, TSTime64 tFrom);
        virtual int GetData(float*& pData, CSRange& r, TSTime64& tFirst, const CSFilter* pFilter = nullptr) const;
        virtual TSTime64 PrevNTime(CSRange& r, const CSFilter* pFilt = nullptr) const;
        virtual void NewDataRead(){m_pBack = nullptr;}   // Opportunity to clean any cached data

        // Routines for Wave blocks
        virtual int ChangeWave(const float* pData, size_t count, TSTime64 tFrom, size_t& first);
    };
}
#endif