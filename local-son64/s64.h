// s64.h
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

#ifndef __S64_H__
#define __S64_H__
//! \file
//! \brief Basic public definitions for the 64-bit son file system
/*!
\defgroup GpFile File commands
\brief Commands that apply to CSon64File objects

These functions all operate at the file level for operations such as opening existing
files and discovering the file contents and to create new files and set the file
properties.
*/
/*! \defgroup GpChan Channel commands
\brief General channel commands

These functions all operate on channels of more or less any type, or on channel of more
than one type. Channel commands that are for specific channel types are grouped with
the channel type.
*/

/*!
\defgroup GpBuffering Channel buffering commands
\brief Channel buffering commands (retrospective buffering)

These commands are used to control the buffering used when writing a new data file. You can
also use these commands to enable/disable saving data and to retrospectively save data in the
recent past.
*/

/*!
\defgroup GpEvent Event channel commands
\brief Commands associated with EventRise and EventFall channels

These commands relate to creating, reading and writing event channels. You can also use the
reading commands to read Marker or extended Marker channels as event channels.
*/

/*!
\defgroup GpLevel Level event commands
\brief Commands associated with EventBoth (Level) channels

These commands are used to create, read and write EventBoth channels, treating them as a
stream of event times that alternate between high and low states. In the \ref Son64 library,
EventBoth channels are implemented as Marker data. These routines are provided for convenience
and for backwards compatibility with the \ref Son32 which treated EventBoth channels as
events and stored the polarity in each data block.
*/

/*!
\defgroup GpMarker Marker channel commands
\brief Commands associated with Marker channels

These commands are used to create, read and write Marker data. You can also use the reading
commands to read extended Marker data as if it were Marker data. The CSon64File::EditMarker()
command can also be used with extended Marker channels.
*/

/*!
\defgroup GpExtMark Extended marker channel commands
\brief Commands associated with extended Marker channels

Extended marker channels handle \ref TextMarkData, \ref RealMarkData and \ref AdcMarkData.
These commands are used to create, read and write these channels. You can also use the
CSon64File::EditMarker() command to modify these channels.
*/

/*!
\defgroup GpWave Waveform channel commands
\brief Commands associated with waveform channels

These commands are used to create, read and write \ref Waveform channels.
*/

#define S64_OS_WINDOWS  1		//!< Value to set in S64_OS to indicate Windows
#define S64_OS_LINUX    2		//!< Value to set in S64_OS to indicate Linux

#if (defined(__linux__) || defined(_linux) || defined(__linux))
#define S64_OS S64_OS_LINUX     //!< The OS to build non-portable code for
#else
#define S64_OS S64_OS_WINDOWS	//!< The OS to build non-portable code for
#endif

#if (S64_OS == S64_OS_WINDOWS) && !defined(_DEBUG)
#define _SECURE_SCL 0	        //!< if not debug build, no range check in STL
#endif

// Standard library includes BEFORE we define DllClass
#include <cstdint>
#include <algorithm>
#include <array>
#include <string>

// Operating system specific includes done here. If you need to define a
// DllClass macro, do it after any other includes. If you do not define it,
// we define it for you as nothing.
#if   S64_OS == S64_OS_WINDOWS
#include <windows.h>
// To stop Microsoft min and max stopping std::min and std::max from working
#undef min
#undef max

#ifndef S64_NOTDLL
#ifdef DLL_SON64
#define DllClass __declspec(dllexport)	//!< Mark as items that are exported
#else
#define DllClass __declspec(dllimport)	//!< Mark as items that are imported
#endif
#endif

#elif S64_OS == S64_OS_LINUX
#include <fcntl.h>      /* open, open64 */
#include <unistd.h>     /* close, read, lseek, lseek64, fsync */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>

#ifndef S64_NOTDLL
#ifdef GNUC
    #define DllExport __attribute__((dllexport))
    #define DllImport __attribute__((dllimport))
#else
    #define DllExport
    #define DllImport
#endif
#endif

/* Linux (and Unix) do not usually define O_BINARY as they do not need to mangle text */
#ifndef O_BINARY
#define O_BINARY 0
#endif
#endif

#ifndef DllClass
#define DllClass
#endif

//! Value used for Maximum possible time in a 64-bit son file.
/*!
This is deliberately NOT the maximum possible int64_t value. We back off by 1/8 of the
maximum posible (backing off by a HUGE time). This allows us to not worry about results
of time calculations overflowing for all practical purposes (unless something has gone
really badly wrong). In the 32-bit SON library, every time calculation is (or should
be) followed by an overflow check for a negative result.
*/
#define TSTIME64_MAX (INT64_MAX-(INT64_MAX>>3))

//! Namespace used for public items from the library
namespace ceds64
{
	//! TSTime64 type used to describe times in ticks (file time base units).
	/*!
	 Ticks are the indivisible time units of the library. All items in a file are at
     times in the range 0 to TSTIME64_MAX-1 ticks. Values equal or larger that
	 the limit will still work in more or less all circumstances; the limit is there
     so we can skip checking for wraparound every time we add a small value to a time.
	 
	 We also use the time value -1 to mean that a searched for time was not found. This
     should not be confused with error code -1 (NO_FILE). We presume if you got far
     enough to search, the file is open.

     To get a time in seconds, multiply the TSTime64 value by CSon64File::GetTimeBase().
	 */
    typedef int64_t TSTime64;

    typedef std::array<uint8_t,8> TMarkBytes; //!< Marker information as 8 unsigned bytes
    typedef std::array<int32_t,2> TMarkInts;  //!< Marker information as 2 32-bit integers

    //! The base class for all marker and extended marker types
    /*!
     Think very carefully before adding constructors to this type and descendants.
     They are used when allocating vectors and arrays and could slow down the code.
     The TMarker type is used for both Marker data and for EventBoth.

	 The Marker is a time stamp, plus an additional 8 bytes that can be used as eight
	 marker codes, two int32_t items, one 64-bit item, or (as is usually the case) four
     marker codes (for compatibility with the 32-bit library) and a spare 32-bit integer
     that is currently always set to 0. This spare space is reserved for a future use.
    */
    struct TMarker
    {
        TSTime64 m_time;         //!< the time stamp of this marker
        union
        {
            TMarkBytes m_code;  //!< Marker codes (always 4, can be 8 or m_int[1] available)
            TMarkInts  m_int;   //!< Integer codes in the same space as m_code
            int64_t    m_int64; //!< All marker codes as one 64-bit integer (as a convenience)
        };

        //! Initialise this object to a known time and code, and 0 remaining space
		/*!
		 * We cannot declare a constructor as this causes TDataBlock to fail as it
		 * holds a union of various objects, including TMarker.
		 * \param t		The time to set in the marker
		 * \param code	Set m_int[0] to this, m_int[1] is set to 0
		 */
        void Init(TSTime64 t, int code = 0){m_time = t; m_int[0] = code; m_int[1] = 0;}

        operator TSTime64() const {return m_time;}	//!< Allows TMarker to be used as a time

        //! Compare TMarkers by time (for example when sorting)
        /*!
        \param rhs The marker to compare with
        \return    true if this marker's time is less than rhs's time.
        */
        bool operator<(const TMarker& rhs) const
        {
            return m_time < rhs.m_time;
        }

        //! Compare the time held in the marker with an arbitrary time
        /*!
        \param  t The time to compare against
        \return true if this marker's time is less than t.
        */
        bool operator<(TSTime64 t) const
        {
            return m_time < t;
        }
    };

    //! Base type for the extended marker types.
    /*!
     The actual objects are dynamically sized. It will always be an error to
     use sizeof() on this type or on any derived type unless we add a complete
     type at some later date.
     */
    struct TExtMark : public TMarker
    {
    };

    //! A type to express the concept of a TextMark.
    /*!
     The m_data member is here so we can take its address. The actual object is
     dynamically sized; this type is a convenience to get the starting address of
	 the data and does not represent the entire item.
    */
    struct TTextMark : public TExtMark
    {
        char m_data;                        //!< start of space for text, 0 terminated
        char* Chars() {return &m_data;}     //!< Get the start of the data as char*
        const char* Chars() const {return &m_data;} //!< Get the start of the data as const char*
    };

    //! A type to express the concept of a RealMark.
    /*!
     The m_data member is here so we can take its address. The actual object is
     dynamically sized; this type is a convenience to get the starting address of
	 the data and does not represent the entire item.
    */
    struct TRealMark : public TExtMark
    {
        float m_data;                       //!< The start of space for data
        float* Floats() {return &m_data;}   //!< Get the start of the RealMark attached data as float*
        const float* Floats() const {return &m_data;}   //!< Use this to get the start of the data
    };

    //! A type to represent the concept of a WaveMark.
    /*!
     The m_data member is here so we can take its address. The actual object is
     dynamically sized; this type is a convenience to get the starting address of
	 the data and does not represent the entire item.
    */
    struct TAdcMark : public TExtMark
    {
        short m_data;                       //!< start of space for data
        short* Shorts() {return &m_data;}   //!< Get the start of the waveform data as a short *
        const short* Shorts() const {return &m_data;}   //!< Use this to get the start of the data
    };

    typedef uint16_t TChanID;   //!< unique channel identifier number
    typedef uint8_t TBlkLevel;  //!< 0=data, >0 level of index
    typedef char* s64path;      //!< path to a data file

#if   S64_OS == S64_OS_WINDOWS
    typedef int64_t TDiskOff;	//!< Type to represent an offset in the data file on disk
    typedef HANDLE TS64FH;		//!< Type to represent the handle of a data file     
#elif S64_OS == S64_OS_LINUX
    typedef off64_t TDiskOff;	//!< Type to represent an offset in the data file on disk
    typedef int TS64FH;			//!< Type to represent the handle of a data file  
#endif

    typedef uint16_t TChanNum;  //!< A channel number, in the range 0-65535

    //! Time and date of sampling
    /*!
     Ideally, this holds the time and date at which sampling started. It can
     be left set to 0's, meaning no time was set. This is bit-compatible with
     the TSONTimeDate structure used by the 16-bit SON library.
    */
    struct TTimeDate            // bit compatible with TSONTimeDate
    {
        uint8_t ucHun;          //!< hundreths of a second, 0-99
        uint8_t ucSec;          //!< seconds, 0-59
        uint8_t ucMin;          //!< minutes, 0-59
        uint8_t ucHour;         //!< hour - 24 hour clock, 0-23
        uint8_t ucDay;          //!< day of month, 1-31
        uint8_t ucMon;          //!< month of year, 1-12
        uint16_t wYear;         //!< year 1980-65535! 0 means unset.

        //! Sets the contents to 0
        void clear(){ucHun=ucSec=ucMin=ucHour=ucDay=ucMon=0; wYear=0;}
    };

    //! Identifies the creating application, can be empty
    /*!
    This is defined to be bit compatible with TSONCreator in the 32-bit son library.
    If you use this you should fill in characters from the left and either pad to
    the right with spaces or pad on the right with (char)0. We expect this to be
	printable characters (null terminated if less than 8) that identify the data
	creator (for example "Spike8").
    */
    struct TCreator
    {
        std::array<char, 8> acID;           //!< to identify the creating app
        //! Sets the contents to 0
		void clear() {acID.fill(0);}
    };

    //! 64-bit son library error codes
    /*!
    All codes are negative except for S64_OK, which is 0.
    */
    enum S64_ERROR
    {
        S64_OK = 0,             //!< Operation completed without detected error
        NO_FILE  = -1,          //!< attempt to use when file not open
        NO_BLOCK = -2,          //!< failed to allocate a disk block
        CALL_AGAIN = -3,        //!< long operation, call again (should be internal use only)
        NO_ACCESS = -5,         //!< not allowed, bad access, file in use
        NO_MEMORY = -8,         //!< out of memory reading 32-bit son file
        NO_CHANNEL = -9,        //!< Channel does not exist
        CHANNEL_USED = -10,     //!< attempt to reuse a channel that already exists
        CHANNEL_TYPE = -11,     //!< channel cannot be used for this operation
        PAST_EOF = -12,         //!< read past the end of the file
        WRONG_FILE = -13,       //!< Attempt to open wrong file type
		NO_EXTRA = -14,			//!< request is outside extra data region
        BAD_READ = -17,         //!< read error
        BAD_WRITE= -18,         //!< write error
        CORRUPT_FILE = -19,     //!< file is bad or attempt to write corrupted data
        PAST_SOF = -20,         //!< attempt to access before start of file
        READ_ONLY= -21,         //!< Attempt to write to read only file
        BAD_PARAM= -22,         //!< a bad parameter to a call
        OVER_WRITE= -23,        //!< attempt to over-write data when not allowed
        MORE_DATA= -24,         //!< file is bigger than header says; maybe not closed correctly
        S64_MINERROR = MORE_DATA
    };

    //! Constants defining channels, revisions and the like
    /*!
    REV_MINOR changed from 0 to 1 July 2017 to warn that header extension count
    could be wrong for files with lots of channels and strings in minro rev 0.
    */
    enum
    {
        MINCHANS = 32,          //!< minimum channels in a file
        MAXCHANS = 2000,        //!< some (arbitrary) limit on the number of channels
        MAXHEADUSER = 65536,    //!< maximum user space in the header (arbitrary)
        REV_MAJOR = 1,          //!< major revision number, first is 1
        REV_MINOR = 1,          //!< minor revision number, up to 99
        TEXTMARK_MIN = 8,       //!< if smaller, increase to this size
        TEXTMARK_MAX = 4000,    //!< arbitrary limit on size of text mark
        NUMFILECOMMENTS = 8,    //!< Number of file comments available
    };

#if S64_OS == S64_OS_WINDOWS
#pragma warning(push)
#pragma warning(disable: 4480)
#endif
    //! The type of a data channel
    /*!
     We may wish to add ExtMark for marker types that are beyond the ones we
     have defined here. The number defined here match those defined for the 16-bit
     SON library.
    */
    enum TDataKind : uint8_t
    {
        ChanOff=0,          //!< the channel is OFF
        Adc,                //!< a 16-bit waveform channel
        EventFall,          //!< Event times (falling edges)
        EventRise,          //!< Event times (rising edges)
        EventBoth,          //!< Event times (both edges)
        Marker,             //!< Event time plus 4 8-bit codes
        AdcMark,            //!< Marker plus Adc waveform data
        RealMark,           //!< Marker plus float numbers
        TextMark,           //!< Marker plus text string
        RealWave            //!< waveform of float numbers
    };
#if S64_OS == S64_OS_WINDOWS
#pragma warning(pop)
#endif
    class CSFilter;

    //! Flags to set when opening a data file
    /*!
     In most cases, when opening an old file there is no need for special flags. However, we
     may want to allow shared access in the future, so we define a flag for this now. We also
     define a flag for test access so that you can open a file, even if the header is corrupt.
     This is to allow us to use the library when attempting to scan or fix a damaged file. The
     flags are bits, so test by (flags & eOF_test) != 0, for example.
    */
    enum eOpenFlags
    {
        eOF_none = 0,       //!< to make it easy to set no flags
        eOF_shared = 1,     //!< Reserved for opening shared, not yet implemented
        eOF_test = 2,       //!< If set, open the file even if verify of the file head fails
    };

    //! flags to set when calling Commit
	/*!
	 Beware that setting eCF_flushSys on Windows systems has been observed to cause the
     system to become unresponsive for several seconds while all dirty data buffered on
     the disk is written. We suspect that this is because modern disks may implement very
     large data buffers. They know which blocks in the buffers are dirty (need writing),
     but may not know which file each block belongs to. When we tell the OS to commit a
     file with eCF_flushSys set, the system tells the disk to write all dirty blocks, and
     this can be a lot of data.

	 Ironically, older disks that rely on the OS to handle all buffering work better
     for a commit as the OS can write only dirty blocks belonging to the data file.
	 
	 If you do not set this flag, Commit() writes all unwritten buffered data held by the
     SON library to the operating system. What happens to it then depends on the OS.
     Once handed off to the OS, it will be written to the disk even if the writing
     application crashes as long as the OS survives.
	 */
    enum eCommitFlags
    {
        //! After writing data, tell system to ensure it is physically on the disk (SLOW).
        /*!
        Only use this if you are certain you can tolerate the disk system becoming inaccessible
        for several seconds. You might use this just before closing a newly created file to be
        certain that it is safely saved.
        */
        eCF_flushSys = 1,

        //! Only commit the file header, not buffered channel data
        /*!
        You would use the eCF_headerOnly flag if you added or deleted a channel or changed
        any file information and wanted to be certain that the file header was committed with
        the change. The file header also holds all text strings used in the file (even channel
        comments, titles and units)
        */
        eCF_headerOnly = 2,

        //! Kill off channel buffering after committing data
        /*!
        This will set any circular buffer set for the channel to have 0 size. If you use this
        flag with the eCF_headerOnly flag you will lose any data that was in the circular buffer.
        */
        eCF_delBuffer = 4,
    };

    //! The interface to the data file
    /*!
    This is a virtual interface which is intended to be overridden. In the first instance, this
    is implemented by two classes: TSon64File (the new 64-bit son file) and TSon32File (an
    implementation of the old 32-bit son file using the new API). In theory, you could interface
    to other filing systems (at least for reading) by implementing this API for them.

    When the documentation refers to sizes and limits, these are for the native 64-bit library.
    When dealing with other emulated formats there will be other limits and limitations. For example,
    it is much more likely that you can read a different format file with this API than you can write
    it.

    The write calls use size_t for the number of items to write. The read calls use an int (a 32-bit integer).
    Although this is asymmetric, the read calls use an int as the return type is an int with negative numbers
    being reserved for errors.
    */
    class DllClass CSon64File
    {
    public:
        CSon64File(){};
        virtual ~CSon64File(){};

        //! Create a new, empty file on disk
        /*!
        \ingroup GpFile
        When you create a new file it is assumed that you will be writing to it. To this end, any
        channels are created with additional buffering that allows you to control the flow of data
        to disk (you can send data to the file and then choose if it is permanently saved - see the
        Save() and SaveRange() comamnds). The default state is to save all data.

        The new file is created with no file creator, no time and date, a tick period (time resolution)
        of 1 us, no file comments.

        \sa Open(), Close(), SetBuffering(), Save(), SaveRange(), Commit()
        \param name   The path and name of the file. This follows the usual naming conventions of
                       the operating system. If \_UNICODE is defined, there are two instances of this
                       function: one using wide strings and one with using narrow strings. Narrow
                       strings are assumed to be coded as UTF-8.
        \param nChans The initial number of channels. It is theoretically possible to increase the
                       number of channels later, but there is not yet code to do this. The minimum
                       number is 32. There is an upper limit of MAXCHANS (1000) but this is an
                       arbitrary limit and could be increased. However, we only allow up to 128 header
                       blocks, which imposes some limit on the absolute maximum number.
        \param nFUser Space in bytes to reserve for binary user data accessed by SetExtraData()
                       and GetExtraData(). Default value is 0. The current limit is MAXHEADUSER which
                       is 65536 bytes. This limit is arbitrary, though we are not aware of this area ever
                       being used. See SetExtraData() and GetExtraData().
        \return       0 if created OK. Otherwise a negative error: BAD_PARAM, NO_ACCESS, NO_FILE,
                       BAD_WRITE. If these is an error and a file was opened, it is closed.
        */
        virtual int Create(const char* name, uint16_t nChans, uint32_t nFUser = 0) = 0;

#ifdef _UNICODE
        //! Create a new empty file on disk
        /*!
        Wide character version for use in \_UNICODE build.
        \copydoc Create(const char*,uint16_t,uint32_t)
        \ingroup GpFile
        */
        virtual int Create(const wchar_t* name, uint16_t nChans, uint32_t nFUser = 0) = 0;
#endif

        //! Open an existing file
        /*!
        \ingroup GpFile
        This command opens an existing data file for reading or reading and writing. If you open a
        file in read-only mode, you cannot use any commands that would change the data.

        When you use this command, even if you allow writing, channels do not have the additional
        buffering that allows you to write data, then decide not put it in the disk file. There is
        up to one block of buffering done on written data, but all written data will end up on disk.
        SetBuffering() has no effect.

        When a file is open in read only mode, we do not stop you making changes to things in memory,
        but nothing gets written to disk. This is a convenience to allow you to change things like the
        time base of a file opened in read only mode.

        Normally, if there is any error, the file is closed. However, if eOF_test is set in flags, if
        the return value is anything other than NO_FILE, we leave the file open and in the 64-bit
        file case you can read binary data using TSon64File::Read().
        WRONG_FILE means we did not recognise the file header. CORRUPT_FILE
        means we read the header, but that the string store or channel information was bad. PAST_EOF means
        that something in the header was positioned past the header size.  MORE_DATA means we have a 
        valid file head, but there are extra data blocks on the end, probably not closed properly.
        BAD_READ means we had a disk problem of some kind.

        \sa Create()
         \param name      Path to the file
         \param iOpenMode 1=read only, 0=read/write, -1 readwrite, then try readonly
         \param flags     Flags to control more aspects of opening, such as test mode.
         \return          S64_OK (0) if done, else a negative error code.
        */
        virtual int Open(const char* name, int iOpenMode = 1, int flags = ceds64::eOF_none) = 0;

#ifdef _UNICODE
        /*!
        Wide character version for use in the \_UNICODE build.
        \copydoc Open(const char*,int,int)
        \ingroup GpFile
        */
        virtual int Open(const wchar_t* name, int iOpenMode = 1, int flags = ceds64::eOF_none) = 0;
#endif

        //! Close an open file
        /*!
        \ingroup GpFile
        This calls Commit() and FlushSysBuffers() (as long as there is a file). If we think that
        the file is open we always close it, even if Commit() returns an error.
        \return Returns NO_FILE if we think that the file was not open. Otherwise we return
                whatever Commit() returns.
        */
        virtual int Close() = 0;

        //! Given a file that is open for writing, empty channels, leaving the header intact
        /*!
        \ingroup GpFile
        The aim is to mark all the data as gone, but preserve the headers and block list for
        the file. The aim is to allow someone to abandon sampling, and start again to the same
        data file.
        */
        virtual int EmptyFile() = 0;

        //! Report if the file is read only
        /*!
        \ingroup GpFile
        \return true if the file can be written to, false if it is marked read only.
        */
        virtual bool CanWrite() const = 0;

        //! Find the lowest numbered channel that is off
        /*!
        \ingroup GpFile
        The file MUST be open to call this.
        \return The lowest channel number of a channel that is not used or error NO_CHANNEL.
        */
        virtual int GetFreeChan() const = 0;

        //! Make sure all data in memory is passed to the disk system
        /*!
        \ingroup GpFile
        Write any parts of the file that need writing. Note that this just means that the
        data has made it as far as the operating system buffers. Make sure you understand
        how the iFlags parameter works. The default flags value of 0 will make sure that all
        dirty data (in the file header or channel data) is written to the operating system.

        The operating system will likely have its own disk buffers and it is entirely up to
        the operating system to decide when data is physically transferred to the disk. Even
        when data is sent to the disk, this does not guarantee permanent storage as disks have
        their own buffering systems which allow them to reorder writes into the most efficient
        (least head moving) order.

        So with the default flag setting (0), if the application that is writing the data stops
        or hangs up, the data should will still be written to disk when the file closes due to
        the application being tidied up by the operating system. However, should the power fail,
        or the operating system itself crash, the data may not make it to the physical disk.

        You can set the eCF_FlushSys flag. If this is supported on your system it will ask the
        operating system to flush all the data for your file to the disk.  Unfortunately, this
        may also flush _all pending data_ for that disk drive (or even for all disk drives) to
        the disk, which could take _a very long time_ (many seconds). This operation usually
        suspends all other disk drive operations, _so the system effectively stops_. You will
        need to test this on your target system to see how well or badly it works in this case.

        If a channel with a circular buffer has data marked for saving and for not saving,
        on a commit, all data marked for saving is saved. You cannot change the state of
        saving for anything before the time of the last saved data.

        Put another way: commit commits all data marked for saving and any gaps in that
        data. If all the data in the circular buffer is markes as not saving, commit does
        nothing, and you can mark any of it for saving after the commit.

        \param iFlags eCommitFlags flags. BEWARE, eCF_FlushSys can be _very slow_.
        \return S64_OK (0) if no detected problem or a negative error code (BAD_WRITE?)
        */
        virtual int Commit(int iFlags = 0) = 0;

        //! Report if the file has unwritten information in memory
        /*!
        \ingroup GpFile
        If an open data file is modified, it can have unwritten data in the file head and also in
        the data channels. Unwritten data can be written o the file using Commit().
        \return false Not modified, true for modified.
        */
        virtual bool IsModified() const = 0;

        //! Tell the system to move data in disk buffers to the disk
        /*!
        \ingroup GpFile
        Where supported, this tells the OS to make sure that data written to OS buffers ends
        up on disk. This can be very inefficient as it is allowed to remove all data from disk
        caches, resulting in very slow operation until data is reloaded.
        \return S64_OK (0) or an error code.
        */
        virtual int FlushSysBuffers() = 0;

        //! Get the seconds per clock tick
        /*!
        \ingroup GpFile
        Everything in the file is quantified to the underlying clock tick (TSTime64). As
        all values in the file are stored, set and returned in ticks, you need to read 
        this value to interpret times in seconds.
        \return The underlying clock tick period in seconds.
        */
        virtual double GetTimeBase() const = 0;

        //! Set the seconds per clock tick
        /*!
        \ingroup GpFile
        Everything in the file is quantied to the underlying clock tick (TSTime64). If
        you change this value the file header is marked as modified and the next Commit()
        will save it to the file. As all times are saved in terms of ticks of this period,
        nothing else in the file changes, but the entire time base of the file is scaled.
        \param dSecPerTick  The new file time base.
        */
        virtual void SetTimeBase(double dSecPerTick) = 0;

        //! Write binary values to the user-defined space in the file header
        /*!
        \ingroup GpFile
        Data in the user-defined extra data area is treated as pure binary bytes and it is
        up to the application what is stored in this region (which is ignored by the rest of
        the library). This region is limited to a maximum size of MAXHEADUSER; it could be
        increased without breaking the file API.
        \param pData Points at a bloc of binary data to be written.
        \param nBytes The number of bytes to write
        \param nOffset The byte offset into the extra data region at which to write the data.
        \return S64_OK (0) or a negative error code if the transfer does not fit in the
                extra data region.
        */
        virtual int SetExtraData(const void* pData, uint32_t nBytes, uint32_t nOffset) = 0;

        //! Read binary data values from the user-defined space in the file header
        /*!
        \ingroup GpFile
        Data in the user-defined extra data area is treated as pure binary bytes and it is
        up to the application what is stored in this region (which is ignored by the rest of
        the library). This region is limited to a maximum size of MAXHEADUSER; it could be
        increased without breaking the file API.
        \param pData Read buffer for the data.
        \param nBytes The number of bytes to read
        \param nOffset The byte offset into the extra data region from which to read the data.
        \return S64_OK (0) or a negative error code if the transfer does not fit in the
                extra data region.
        */
        virtual int GetExtraData(void* pData, uint32_t nBytes, uint32_t nOffset) = 0;

        //! Get the size of the extra data region
        /*!
        \ingroup GpFile
        \return The size of the extra data region, in bytes.
        */
        virtual uint32_t GetExtraDataSize() const = 0;

        //! Set a file comment
        /*!
        \ingroup GpFile
        There are up to NUMFILECOMMENTS (8) comments stored in the file header. There is
        no limit (save common sense and available space in the header) to the size of each
        comment, and comments can contain end of line characters. The 32-bit system
        limited comments to 79 characters and 5 comments.
        \param n The index of the comment, 0 to NUMFILECOMMENTS-1
        \param szComment A string holding the comment to set.
        \return S64_OK (0) or a negative code (BAD_PARAM if n is out of range).
        */
        virtual int SetFileComment(int n, const char* szComment) = 0;

        //! Get a file comment
        /*!
        \ingroup GpFile
        See SetFileComment() for more information.
        \param n    The index of the comment, 0 to NUMFILECOMMENTS-1
        \param nSz  The size of the \a szComment area.
        \param szComment Can be nullptr to get the length, else space for the string.
        \return     The space needed to read the full string or a negative error. This
                    is not the string length, which could be less. Son32 files return
                    the maximum needed space, Son64 return the exact space needed.
        */
        virtual int GetFileComment(int n, int nSz = 0, char* szComment = nullptr) const = 0;

        //! Get the maximum number of channels in the file
        /*!
        \ingroup GpFile
        The maximum number of channels in a file is set by the Create() call. The channels
        are numbered from 0 up to the maximum number -1.
        \return The maximum numberof channels that can be stored in the file.
        */
        virtual int MaxChans() const  = 0;

        //! Get and/or set an object that identifies the creator of the file.
        /*!
        \ingroup GpFile
        TCreator holds 8 characters that can be used to define the application that
        created the data. Alternatively, it can be left filled with 0s for an anonymous
        file creator. Spike2 sets this item, but there is no harm if you do not.
        If you use the SetExtraData() command you should probably set a creator to
        identify the format of the extra data.
        \param pRead Either points at a TCreator to be read from the file or nullptr if you
                     do not wish to read data.
        \param pWrite Either points at a TCreator to write or nullptr to not write.
        \return S64_OK (0)
        */
        virtual int AppID(TCreator* pRead, const TCreator* pWrite = nullptr) = 0;

        //! Get/Set time and data at which sampling started
        /*!
        \ingroup GpFile
        This routine can get and set the system time at which sampling started. As it is
        up to the application to set this value, there is no guarantee that it is set or
        that even if set, that it is correct. Spike2 does set this value as accurately
        as it can based on the system clock.
        \param pTDGet Either a pointer to the values to read or nullptr if not reading.
        \param pTDSet Either a pointer to the values to write or nullptr if not writing.
        \return 1 if reading and a possible value was read and no write error, 0 if no
                problem on write or if reading and all values read are 0, or -1 if either
                reading or writing and an impossible time value was found.
        */
        virtual int TimeDate(TTimeDate* pTDGet, const TTimeDate* pTDSet = nullptr) = 0;

        //! Get the version of the data file
        /*!
        \ingroup GpFile
        The returned value is the major file system version * 256 + the minor version. The
        major version number of the 32-bit filing system opened through this interface is 0.
        The first major version of the 64-bit filing system is 1, so the returned value is
        a minimum of 256 for a 64-bit file and less for a 32-bit file.
        \return (Major_Rev<<8)+Minor_Rev
        */
        virtual int GetVersion() const = 0;

        //! Get the offset to the next block (units of DBSize) the file would allocate
        /*!
        \ingroup GpFile
        When a file is opened for reading, the physical file size should be the same or up to
        DBSize+DLSize less than this. If it is more, the file has extra information written on
        the end and needs fixing... it was probably interrupted during writing. This does not
        included buffered data that is not committed to disk during writing.
        \return The (estimated) file size in bytes. Should be a multiple of DBSize.
        */
        virtual uint64_t FileSize() const = 0;

        //! Get an estimate of the data bytes stored for the channel
        /*!
        \ingroup GpChan
        This is really only to say if there is any data stored or not for the channel and maybe
        to allow an estimate of the space/time needed for an operation on the channel. This will
        be reasonably accurate as long as all buffers written are full except the last. If you are
        sampling and have circular buffers, uncommitted data in the circular buffers is included.

        To get an upper estimate of the number of data items held by the channel divide the
        returned size by the channel item size ChanBytes(chan)/ItemSize(chan).
        \return Bytes on disk or commited to the disk for the channel.
        */
        virtual uint64_t ChanBytes(TChanNum chan) const = 0;

        //! Get the maximum time of any item in the file
        /*!
        \ingroup GpFile
        The maximum time written to disk is saved in the file head. If this is not set the maximum
        time is found by scanning all the channels for the maximum time saved in any channel. We
        can also force all channels to be scanned with the bReadChans argument. See ExtendMaxTime().
        \param bReadChans If this is true, if any channel has a later time than the file head, the
                channel time is used instead. If false, and the file head holds a time, it is used.
        \return The maximum time in the file in ticks. If the file is empty, this can be
                returned as -1.
        */
        virtual TSTime64 MaxTime(bool bReadChans = true) const = 0;

        //! Extend the maximum time in the file or cancel it
        /*!
        \ingroup GpFile
        The MaxTime() function can find the maximum time in the file by scanning all the channels.
        However, each time we write data we update the file head to hold the last written time. This
        can be used to make a file appear longer than it is.
        \param t    If this is -1 or greater than the current values saved as the maximum time,
                    change the value stored in the file head to this value.
        */
        virtual void ExtendMaxTime(TSTime64 t) = 0;

        //! Get the channel type
        /*!
        \ingroup GpChan
        All channels have a type, even unused ones (ChanOff).
        \return The type of the channel
        */
        virtual TDataKind ChanKind(TChanNum chan) const = 0;

        //! Get the waveform sample interval in file clock ticks
        /*!
        \ingroup GpChan
        This is used by channels that sample equal interval waveforms and is the number of
        file clocks ticks per sample.
        \param chan 
        \return The sample interval in file clock ticks. If you use this on a channel that
                does not exist, the return value is 1.
        */
        virtual TSTime64 ChanDivide(TChanNum chan) const = 0;

        //! Get the "ideal" rate for a channel
        /*!
        \ingroup GpChan
        The ideal rate for a waveform channel should be set to be the desired rate, which will
        often be the reciprocal of the divide (scaled for seconds). The ChanDivide() routine
        will give you the actual rate. For an event-based channel, this value represents the
        expected mean sustained event rate - often used for scaling buffer allocations and also
        for guessing the y axis range for rate-related displays.

         This function gets and/or sets the ideal waveform sample rate for Adc and RealWave
         channels and the expected average event rate for all other channel. This is the dRate
         parameter from the Set... family of routines that create channels.
         \param chan The channel number
         \param dRate If this is >= 0.0 the value will be set. If negative, no change is made.
         \return     The previous value of the ideal rate or 0 if the channel does not exist.
        */
        virtual double IdealRate(TChanNum chan, double dRate = -1.0) = 0;

        //! Get the physical channel number saved with this channel
        /*!
        \ingroup GpChan
        This routine returns the physical channel number that was associated with the channel
        when it was created. The stored value is solely for information, it has no other effect
        in the library.
        \param chan The channel number
        \return     The physical channel number associated with the channel, -1 if it was never
                    set, NO_CHANNEL (negative) if there is no such channel.
        */
        virtual int PhyChan(TChanNum chan) const = 0;

        //! Set the comment text associated with a channel
        /*!
        \ingroup GpChan
        You can associate a comment string with a channel. The length of the string is not limited,
        but the previous 32-bit filing system set a limit of 71 characters; we do not recommend
        setting huge channel comments.

        \sa GetChanComment(), SetChanUnits(), SetChanTitle(), SetFileComment()
        \param chan The channel number (which should exist)
        \param szComment The text to store as the comment.
        \return Either S64_OK (0) or NO_CHANNEL if the channel does not exist.
        */
        virtual int SetChanComment(TChanNum chan, const char* szComment) = 0;

        //! Get the comment text associated with a channel
        /*!
        \ingroup GpChan
        See also SetChanComment(), GetChanUnits(), GetChanTitle(), GetFileComment()
        \param chan The channel number (which should exist)
        \param nSz  The size of the \a szComment area.
        \param szComment Space to store the read back comment, withh be 0- terminated.
        \return     The space needed to return the entire comment including the terminating
                    0 or negative error. For Son32 files, the return is the maximum space,
                    for Son64 it is the exact space.
        */
        virtual int GetChanComment(TChanNum chan, int nSz = 0, char* szComment = nullptr) const = 0;

        //! Set the channel title
        /*!
        \ingroup GpChan
        You can associate a title string with a channel. The length of the string is not limited,
        but the previous 32-bit filing system set a limit of 9 characters; we do not recommend
        setting huge channel titles.

        \sa GetChanTitle(), SetChanUnits(), SetChanComment()
        \param chan The channel number (which should exist)
        \param szTitle The text to store as the title.
        \return Either S64_OK (0) or NO_CHANNEL if the channel does not exist.
        */
        virtual int SetChanTitle(TChanNum chan, const char* szTitle) = 0;

        //! Get the channel title
        /*!
        \ingroup GpChan
        See also SetChanTitle(), GetChanUnits(), GetChanComment()
        \param chan The channel number (which should exist)
        \param nSz  The size of the space pointed at by \a szTitle.
        \param szTitle Where to store the 0-terminated title or nullptr.
        \return     The size of the space needed to read the entire string including
                    the terminating 0 or a negative error. FOr Son32 files, this is the
                    maximum space, for Son64 files it is the extact space.
        */
        virtual int GetChanTitle(TChanNum chan, int nSz = 0, char* szTitle = nullptr) const = 0;

        //! Set the channel scale
        /*!
        \ingroup GpChan
        Channel scales are used to translate between integer representations of values and
        real units. They are used for Adc, RealWave and AdcMark channels and could find uses
        in other situations in the future. When a channel is expressed in user units:

        user units = integer * scale / 6553.6 + offset

        For a RealWave channel, where the channel is already in user units, the scale and
        offset values tell us how to convert the channel back into integers:

        integer = (user units - offset)*6553.6/scale

        The factor of 6553.6 comes about because if you have a 16-bit ADC that spans 10 V, a
        scale value of 1.0 converts between Volts and ADC values. Please do not worry about
        this value... just use the equations and all will be well.

        \sa GetChanScale(), SetChanOffset(), GetChanOffset()
        \param chan The channel number (can be any channel type as long as it exists)
        \param dScale The new scale value to apply. Any finite value except 0 is acceptable.
                      However, using values close to the largest or smallest allowed fp values
                      is stupid and risks over or underflows.
        \return S64_OK (0) or NO_CHANNEL if chan does not exist.
        */
        virtual int SetChanScale(TChanNum chan, double dScale) = 0;

        //! Get the channel scale
        /*!
        \ingroup GpChan
        See SetChanScale() for details.
        \param chan The channel number (can be any channel type as long as it exists)
        \param dScale The channel scale is returned here.
        \return S64_OK (0) or NO_CHANNEL if chan does not exist.
        */
        virtual int GetChanScale(TChanNum chan, double& dScale) const = 0;

        //! Set the channel offset
        /*!
        \ingroup GpChan
        See SetChanScale() for details. See also GetChanScale().
        \param chan The channel number (can be any channel type as long as it exists)
        \param dOffset The new channel offset in user units.
        \return S64_OK (0) or NO_CHANNEL if chan does not exist.
        */
        virtual int SetChanOffset(TChanNum chan, double dOffset) = 0;

        //! Get the channel offset
        /*!
        \ingroup GpChan
        See SetChanScale() for details. See also SetChanOffset().
        \param chan The channel number (can be any channel type as long as it exists)
        \param dOffset The new channel offset in user units.
        \return S64_OK (0) or NO_CHANNEL if chan does not exist.
        */
        virtual int GetChanOffset(TChanNum chan, double& dOffset) const = 0;

        //! Set the channel units
        /*!
        \ingroup GpChan
        You can associate a units string with a channel. The length of the string is not limited,
        but the previous 32-bit filing system set a limit of 5 characters; we do not recommend
        setting long channel unit strings as this will cause a mess when displayed!

        \sa GetChanUnits(), SetChanTitle(), SetChanComment()
        \param chan     The channel number (which should exist)
        \param szUnits  The text to store as the units.
        \return Either S65_OK (0) or NO_CHANNEL if the channel does not exist.
        */
        virtual int SetChanUnits(TChanNum chan, const char* szUnits) = 0;

        //! Get the channel units
        /*!
        \ingroup GpChan
        \sa SetChanUnits(), GetChanTitle(), GetChanComment()
        \param chan The channel number (which should exist)
        \param nSz The size of the space pointed at by \a szUnits.
        \param szUnits Space to hold the 0-terminated string or nullptr.
        \return The size of the space needed to collect the data including the terminating
                0 or NO_CHANNEL if the channel does not exist. For Son32 files, this is the
                maximum size, for Son64 files it is the exact size.
        */
        virtual int GetChanUnits(TChanNum chan, int nSz = 0, char* szUnits = nullptr) const = 0;

        //! Get the time of the last item on the channel
        /*!
        \ingroup GpChan
        This works for both channels read from disk and channels that are being written. It
        returns the time of the last item held in the channel or -1 if no data exists.
        \param chan The channel number
        \returns The last time in the channel or -1 if no data (or file!) or NO_CHANNEL.
        */
        virtual TSTime64 ChanMaxTime(TChanNum chan) const = 0;

        //! Get the time of N items before a given time
        /*!
        \ingroup GpChan
        Event-based channels
        --------------------
        For an event-based channel, we just count events (or filtered events) backwards from
        the i64From time. If N is 1, we get the first event before sTime. Things are a
        little more complicated if a filter is set for a Marker-based channel as we have to
        ignore items that are filtered out. If there are not N in the given time range, the
        return value is -1. In that case, you can find the first event in the time range by
        using ReadEvents(). For large N, searches are much more efficient if there is no
        filter as we can often avoid looking at individual item times.

        Waveform-based channels
        -----------------------
        This returns the start time at which a read of N waveform points would end with the
        first waveform point in the file before sTime. This means that the read might not get
        N points if there are gaps in the data.
        This returns the latest time at which the last item of a read of N items would end
        before the nominated time if there are no gaps from the start of the read to the sTime.
        If there are gaps, this will return a time just after a gap This may mean that a waveform read would not get N items. Things are much more
        complicated with waveforms as there can be gaps. To find a contiguous range of N items
        you will have to iterate with this command and ReadWave().

        \param chan The channel to use (which must exist)
        \param tStart The time before which we are to search.
        \param tEnd  The earliest time to search.
        \param n     The number of points backwards to search. If you need more than 32-bits worth
                     of search you will have to loop this... and ask yourself why so many!
        \param pFilter If this is a marker or derived type, you can use a filter to limit the
                     searched data.
        \param bAsWave If this is an extended marker type with a channel divide set, then the
                     data is to be treated as a wave, not as events.
        \return -1 if no such item found, a negative error code (NO_CHANNEL) or the time.
        */
        virtual TSTime64 PrevNTime(TChanNum chan, TSTime64 tStart, TSTime64 tEnd = 0,
                                   uint32_t n = 1, const CSFilter* pFilter = nullptr, bool bAsWave = false) = 0; 

        //! Delete a channel
        /*!
        \ingroup GpChan
        This deletes a channel from the file. In a 64-bit file, channels are deleted in such a way that
        as long as you do not reuse the channel, it is possible to undelete them.

        \sa ChanUndelete()
        */
        virtual int ChanDelete(TChanNum chan) = 0;

        //! Used as an argument to ChanUndelete() to determine the action it takes
        enum eCU
        {
            eCU_kind = 0,   //!< Request the channel kind, but take no action
            eCU_restore     //!< ATtempt to restore (undelete) the channel
        };

        //! Undelete a channel
        /*!
        \ingroup GpChan
        This command deals with undeleting a channel. To find if you can undelete a channel you ask for
        the old channel type, which will be ChanOff if you cannot delete it as in use or not deleted.
        Otherwise it will be the type of the channel that you can undelete.
        \param chan   The channel number to inquire about or undelete.
        \param action Either eCU_kind to ask for the type of the deleted channel or eCU_restore
                      to attempt to undelete it.
        \return 0 if all OK, otherwise a negative error code.
        */
        virtual int ChanUndelete(TChanNum chan, eCU action=eCU_kind) = 0;

        //! Set the suggested Y range for a channel
        /*!
        \ingroup GpChan
        The channel Y range need not be used and is here as a convenience for the application. In fact
        you can use these values for any purpose you like as they are not used within the library.
        We suggest that if you do use these values, they are a hint as to the range of data, where this
        is appropriate. For an event channel these could be used for the expected rate range. The
        values are set by default to 1.0 and -1.0. It is an error to set the low and high values
        the same.

        \sa GetChanYRange()
        \param chan     The channel number
        \param dLow     The low value
        \param dHigh    The high value
        \return S64_OK (0) if no error, or BAD_PARAM, NO_CHANNEL.
        */
        virtual int SetChanYRange(TChanNum chan, double dLow, double dHigh) = 0;

        //! Get a suggested Y range for the channel
        /*!
        \ingroup GpChan
        \param chan     The channel number
        \param dLow     A double to hold the low value
        \param dHigh    A double to hold the high value
        \return S64_OK  (0) if no error, or BAD_PARAM, NO_CHANNEL.
        \sa SetChanYRange()
        */
        virtual int GetChanYRange(TChanNum chan, double& dLow, double& dHigh) const = 0;

        //! Get the byte size of the repeating object held by the channel
        /*!
        \ingroup GpChan
        Each channel holds repeating objects, all the same size. Note that all extended
        marker types have sizes that are rounded up to a multiple of 8 bytes. This routine
        is a convenient way to get the object sizes (though you could calculate them). The
        non-Marker derived classes have fixed sizes, but these are also reported:

         channel type | size 
        ------------- | ------
         Adc          |   2
         RealWave     |   4
         EventRise    |   8
         EventFall    |   8
         EventBoth    |  16
         Marker       |  16
         RealMark     |  16 + nRows * nCols * 4
         TextMark     |  16 + nRows
         AdcMark      |  16 + nRows * nCols * 2

        \param chan The channel number.
        \return NO_CHANNEL or the size in bytes. The sizes of all extended marker types are rounded
                up to the next multiple of 8 bytes.
        */
        virtual int ItemSize(TChanNum chan) const = 0;

        //! Set the Save/not save state for a buffered channel
        /*!
        \ingroup GpBuffering
        This applies only to channels that have been set to buffered mode. When in buffere mode, data
        written to the channel is placed in a circular buffer and when the buffer is full, data is then
        written to disk to make space for new incoming data. This call and SaveRange() allows you to not
        do the write to disk for specified time ranges. The data is still available for display and
        calculation as long as it is in the circular buffer.

        Note that adding a save marker to a channel that is already saving at that time has no effect,
        likewise adding a don't save at a point where we are not saving has no effect. That is we keep
        the minimum list of changes. Further, you cannot make a change before the time of the last data
        that has been written. That is, once data is moved from the circular buffer to the write buffer
        you can no loger choose to change the save state for that data or data before that time.

        \sa SaveRange(), IsSaving(), SetBuffering(), LatestTime()
        \param chan A channel in the file. If it is not using buffering this has no effect.
        \param t    The time from which the state set by bSave should take effect. This time can be
                    before or after the current maximum time for the channel. The library saves a list
                    of times at which save/no save changes are to occur.
        \param bSave Set true to save data at this time, false to stop saving.
        */
        virtual void Save(int chan, TSTime64 t, bool bSave) = 0;

        //! Set a time range to be saved
        /*!
        \ingroup GpBuffering
        This is expected to be used with a channel that is normally not being saved. It marks a time
        range for saving by adding a Save and a don't save to the list of save state changes. You
        cannot change the state before the time of the last data sent to the write buffer.

        \sa Save(), IsSaving(), SetBuffering(), LatestTime()
        \param  chan A buffered channel in the file.
        \param tFrom The start of a time range to be saved to disk.
        \param tUpto Data at tUpto onwards will not be saved.
        */
        virtual void SaveRange(int chan, TSTime64 tFrom, TSTime64 tUpto) = 0;

        //! Report the save state at the given time
        /*!
        \ingroup GpBuffering
        If you use this with a non-buffered channel the result is always true. If you
        ask about a channel that does not exist, the result is always false. We can only
        say false if the data is in the circular buffer and is marked for non-saving. Once
        data is too old to be in the circular buffer we assume it is saved as if it was not,
        it will fail to be read from disk.
        \param chan The channel to enquire about
        \param tAt  The time at which we want to know about saving
        \return true if data written to the channel at the nominated time is currently
                scheduled to be saved (or is in the circular buffer and has been saved).
        */
        virtual bool IsSaving(TChanNum chan, TSTime64 tAt) const = 0;

        //! Get a list of times where saving is turned off and on
        /*!
        \ingroup GpBuffering
        This returns the list of times held against the circular buffer which determine what
        data is saved when it is ejected from the circular buffer system. The list is kept for
        all the data in the circular buffer even if it has been committed to disk. This is
        only a snapshot of the state; it is used when drawing to indicate which data is being
        saved and should only be used as a guide.
        \param chan   The channel to query
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
        virtual int NoSaveList(TChanNum chan, TSTime64* pTimes, int nMax, TSTime64 tFrom = -1, TSTime64 tUpto = TSTIME64_MAX) const = 0;

        //! Tell circular buffering the latest time we have reached during data sampling
        /*!
        \ingroup GpBuffering
        With circular buffering in use on a channel, if no data appears for a long time and
        no Commit() commands are issued, the commands to save and not save data ranges
        acumulate. By telling a channel the latest time for which no new data can be
        expected, the channel can clean up the save/no save list by deleting/amalgamating
        ranges that have no data in them.

        \sa Save(), SaveRange(), SetBuffering(), LatestTime()
        \param chan The channel number of a buffered channel or -1 for all channels.
        \param t    All data for this channel up to this time is complete, there will be NO more data
                    written to this channel before this time.
        \return S64_OK (0) or a negative error code (NO_CHANNEL)
        */
        virtual int LatestTime(int chan, TSTime64 t) = 0;

        //! Set up the buffering requirements when writing a new data file with circular buffers.
        /*
        \ingroup GpBuffering
        This function can be used after using Create() to generate a new data file. It allocates
        buffer space for a channel so that the Save() family of commands can be used. You can
        set buffering based on time, in which case the space required is estimated from the rate set
        for the channel, or you can define the number of bytes to be used.

        This can only be called if the channels have not got any data in them as this destroys
        the channel data. It is intended for use before any writing thread has been started.
        If this is called for all channels, space is allocated based on the channel rates and
        the size of the objects stored.

        As the 32-bit son file needs this to be called, it should be called after creating a channel or
        channels. 

        \sa Save(), SaveRange(), IsSaving(), LatestTime(), Create()
        \param chan     Either a channel number or -1 for all channels
        \param nBytes   The space to allocate, or if dSeconds is > 0, the maximum space to allocate.
        \param dSeconds The maximum time to buffer. This is used when sampling data. If chan is -1
                        we save this time for use when setting buffering for other channels created during
                        sampling if nBytes is 1.
        \return         The resulting equivalent buffering time in seconds.
        */
        virtual double SetBuffering(int chan, size_t nBytes, double dSeconds = 0.0) = 0;

        //! Create an EventFall or an EventRise channel
        /*!
        \ingroup GpEvent
        Events are TSTime64 (64-bit integer) time stamps. In the 64-bit son library they do not include
        EventBoth data, which is stored as a Marker channel and should be created with SetLevelChan().
        
        \sa SetLevelChan(), SetMarkerChan(), WriteEvents(), ReadEvents(), SetBuffering()
        \param chan     The new channel to create. The Channel must be of type ChanOff (unused). It can be a
                        channel that has been deleted.
        \param dRate    The expected maximum sustained (over several seconds) event rate. This is used when
                        allocating buffer space. It is also accessible with IdealRate().
        \param evtKind  The type of the channel, either EventRise or EventFall. We will accept EventBoth and this
                        is then converted to a call to SetLevelChan().
        \param iPhyCh   The physical channel number associated with the channel. This is for information only.
        \return S64_OK  (0) if no error is detected, otherwise a negative error code (READ_ONLY, NO_CHANNEL,
                        CHANNEL_USED).
        */
        virtual int SetEventChan(TChanNum chan, double dRate, TDataKind evtKind = EventFall, int iPhyCh=-1) = 0;

        //! Write event data to event channel
        /*!
        \ingroup GpEvent
        Write EventFall or EventRise data to an event channel. Use WriteLevels() for an EventBoth
        channel. The data must be after all data written previously to the channel. The data must
        be in ascending time order (or the file will be corrupt).

        \sa SetEventChan(), ReadEvents(), SetBuffering(), Commit()
        \param chan  An EventFall or EventRise channel.
        \param pData A buffer of data to be written
        \param count The number of event times to write
        \return S64_OK (0) or a negative error code (READ_ONLY, NO_CHANNEL, BAD_WRITE)
        */
        virtual int WriteEvents(TChanNum chan, const TSTime64* pData, size_t count) = 0;

        //! Read event times from any suitable channel
        /*!
        \ingroup GpEvent
        You can read event times from any event channel, marker channel or extended marker channel.
        You can use this to read EventBoth channels, which allows you to select rising or falling
        events by setting a filter.
        
        \sa SetEventChan(), WriteEvents(), ReadLevels()
        \param chan  A suitable channel.
        \param pData A buffer to accept the data of size at least nMax.
        \param nMax  The maximun number of events to read
        \param tFrom Earliest time we are interested in
        \param tUpto One tick beyond the times we are interested in (up to but not including)
        \param pFilter Either nullptr or points at a filter to use when reader Marker or extended
                       marker data as events.
        \return The number of events read or a negative error code
        */
        virtual int ReadEvents(TChanNum chan, TSTime64* pData, int nMax, TSTime64 tFrom, TSTime64 tUpto, const CSFilter* pFilter = nullptr) = 0;

        //! Create a Marker or a EventBoth channel
        /*!
        \ingroup GpMarker
        Markers are TSTime64 (64-bit integer) time stamps plus marker codes. In the 64-bit son library
        EventBoth data is stored as a Marker, not as an event, using code 0 in the first code to mean low
        and any other code to mean high. Markers can be filtered using the codes.
        
        \sa SetLevelChan(), SetMarkerChan(), WriteMarkers(), ReadMarkers(), SetBuffering()
        \param chan     The new channel to create. The Channel must be of type ChanOff (unused). It can be a
                        channel that has been deleted.
        \param dRate    The expected maximum sustained (over several seconds) event rate. This is used when
                        allocating buffer space.
        \param kind     The type of the channel, either Marker or EventBoth.
        \param iPhyChan The physical channel number associated with the channel. This is for information only.
        \return S64_OK  (0) if no error is detected, otherwise a negative error code (READ_ONLY, NO_CHANNEL,
                        CHANNEL_USED).
        */
        virtual int SetMarkerChan(TChanNum chan, double dRate, TDataKind kind = Marker, int iPhyChan = -1) = 0;

        //! Write Marker data to a Marker or EventBoth channel.
        /*!
        \ingroup GpMarker
        You can also use Use WriteLevels() for an EventBoth channel. The data must be after
        all data written previously to the channel. The data must be in ascending time order
        (or the file will be corrupt).

        \sa SetMarkerChan(), ReadMarkers(), SetBuffering(), Commit()
        \param chan  An EventBoth or marker channel.
        \param pData A buffer of data to be written. Times must be in ascending order and the
                     data must occur after any existing data in the channel.
        \param count The number of event times to write
        \return S64_OK (0) or a negative error code (READ_ONLY, NO_CHANNEL, BAD_WRITE)
        */
        virtual int WriteMarkers(TChanNum chan, const TMarker* pData, size_t count) = 0;

        //! Read Marker data from a Marker or extended Marker channel
        /*!
        \ingroup GpMarker
        You can read marker data from any channel that contains markers. Note that you can
        also read a Marker channel as event data with ReadEvents().

        \sa SetMarkerChan(), SetLevelChan(), WriteMarkers((), ReadLevels()
        \param chan A Marker, EventBoth or extended marker channel.
        \param pData A buffer to receive the marker data.
        \param nMax  The maximum number of Markers to return.
        \param tFrom The first time to include in the search for markers.
        \param tUpto The first time not to include. Returned data will span the time range
                     tRom up to but not including tUpto.
        \param pFilter Either nullptr (accept all) or a marker filter to restrict the returned markers.
        \return The number of markers read or a negative error code.
        */
        virtual int ReadMarkers(TChanNum chan, TMarker* pData, int nMax, TSTime64 tFrom, TSTime64 tUpto, const CSFilter* pFilter = nullptr) = 0;

        //! Modify a marker data item already in the channel
        /*!
        \ingroup GpMarker
        This is mainly (exclusively) used to change marker codes of data already written to
        a TMarker-based channel. Currently, we do NOT allow you to change the time, although we
        could do this with some restrictions. At the most, we could allow a time change only back
        to the previous event+1 and up to the next -1.
        \param chan A maker or extended marker channel
        \param t    The time of the existng marker to be modified
        \param pM   The marker object to replace the current object. We stromgly recommend that
                    the time is the same as t (in case we decide to allow time changes in future).
                    However, for now, the time is ignored.
        \param nCopy The number of bytes to copy from the new item. This should be a minimum of
                     sizeof(TSTime64) and can be any value up to the ItemSize()
        */
        virtual int EditMarker(TChanNum chan, TSTime64 t, const TMarker* pM, size_t nCopy = sizeof(TMarker)) = 0;

        //! Create an EventBoth channel (stored as Marker data)
        /*!
        \ingroup GpLevel
        In the 64-bit SON library we store EventBoth data as Marker data using code 0 of the
        first marker code for low and any other code for high. This command sets the initial
        level to low, equivalent to SetInitLevel(chan, false).

        \sa SetMarkerChan(), SetInitLow(), WriteLevels(), ReadLevels(), SetBuffering()
        \param chan     An unused channel in the file.
        \param dRate    The expected maximum sustained event rate (used to allocate buffers)
        \param iPhyChan The physical channel associated with this input (information only)
        \return S64_OK (0) or a negative error code.
        */
        virtual int SetLevelChan(TChanNum chan, double dRate, int iPhyChan = -1) = 0;

        //! Set the initial level of an EventBoth channel
        /*!
        \ingroup GpLevel
        This command only has any effect if used after creating a channel and before any data
        is written with WriteLevels(). It sets the level of the input before the first change
        and so defines the direction of the first change. Of course, if data is written using
        WriteMarkers() there is no effect.

        \sa SetMarkerChan(), SetLevelChan(), WriteLevels(), WriteMarkers()
        \param chan   An existing EventBoth channel
        \param bLevel Set false if the initial level is low, true if it is high. This sets the
                      input level at time -1 ticks and this level is assumed to continue until
                      data is written to the channel.
        \return S64_OK (0) or a negative error code.
        */
        virtual int SetInitLevel(TChanNum chan, bool bLevel) = 0;

        //! Write EventBoth data assuming that each time toggles the level
        /*!
        \ingroup GpLevel
        This command expands the list of EventBoth times into Marker data and writes it to a
        EventBoth channel. The written data must lie after all preceding data and the first
        time is assumed to have the opposite level to the last written level. If no data has been
        written, the first level is assumed to toggle the level set by SetInitLevel(). This command
        is here for compatibility with the 32-bit SON library where EventBoth data was stored as
        event times, not as Markers.

        \sa SetLevelChan(), SetInitLevel(), ReadLevels()
        \param chan  An existing EventBoth channel number.
        \param pData A buffer of event times that are in ascending time order and that occur
                     after any data already written to the file.
        \param count The number of events to add to the file.
        \return      S64_OK (0) or a negative error code (READ_ONLY, NO_CHANNEL, BAD_WRITE)
        */
        virtual int WriteLevels(TChanNum chan, const TSTime64* pData, size_t count) = 0;

        //! Read alternating levels from an EventBoth channel
        /*!
        \ingroup GpLevel
        This is provided for compatibility with the 32-bit SON library. It assumes that the channel
        holds alternating high and low levels and reads it back as events, not Markers.

        \sa SetlevelChan(), SetInitLevel(), WriteLevels()
        \param chan  A existing EventBoth channel.
        \param pData A buffer to hold any returned times.
        \param nMax  The maximum number of times to return.
        \param tFrom The first time to include in the search for levels.
        \param tUpto The first time not to include. Returned data will span the time range
                     tRom up to but not including tUpto.
        \param bLevel The level of the first returned point (true if high, false if low) or if no
                     points are in the time range, the level at time tFrom.
        \return The number of points read or a negative error code.
        */
        virtual int ReadLevels(TChanNum chan, TSTime64* pData, int nMax, TSTime64 tFrom, TSTime64 tUpto, bool& bLevel) = 0;

        //! Create a TextMark channel
        /*!
        \ingroup GpExtMark
        A TextMark is a Marker plus a zero terminated 8-bit character string. You can store UTF-8 text
        in the marker (or anything else you want as the library does not interpret the string.
        The 64-bit SON library rounds up the size of a TextMark to the next multiple of 8 bytes. The
        32-bit SON library rounded up the size to a multiple of 4 bytes. Data is written and read
        using WriteExtMarks() and ReadExtMarks(). Note that as the size of this object is not fixed,
        in general you must deal with pointer arithmetic when indexing arrays of TextMark data.
        You can use ItemSize() to find the size to move points on by and GetExtMarkInfo() where
        the rows is equivalent to the nMax parameter.

        You can create this channel type with the SetExtMarkChan() command, if you wish.

        \sa SetExtMarkChan(), WriteExtMarks(), ReadExtMarks(), ItemSize(), GetExtMarkInfo(),
                  SetBuffering()
        \param chan  An unused channel to create as a TextMark channel.
        \param dRate The expected sustained maximum event rate.
        \param nMax  The number of 8-bit bytes (characters) attached to each TextMark, including the zero byte
                     to terminate the string. In the 64-bit library setting this to a multiple of 8 makes use
                     of all the allocated space.
        \param iPhyChan A physical source channel, used for information only.
        \return S64_OK (0) or a negative error code.
        */
        virtual int SetTextMarkChan(TChanNum chan, double dRate, size_t nMax, int iPhyChan = -1) = 0;

        //! Create a TextMark, RealMark or AdcMark channel
        /*!
        \ingroup GpExtMark
        This command creates and extended marker channel, that is a channel of Markers with attached
        other data. This data is arranged as a grid of rows and columns of data. The data is short
        integers for AdcMark, float values for RealMark and 8-bit characters for TextMark. The 32-bit
        SON library only allowed multiple columns for AdcMark data, but the 64-bit library generalises
        this to all types.

        The 64-bit SON library rounds up the size of an extended marker to a multiple of 8 bytes. The
        32-bit SON library rounded up the size to a multiple of 4 bytes. Data is written and read
        using WriteExtMarks() and ReadExtMarks(). Note that as the size of this object is not fixed,
        in general you must deal with pointer arithmetic when indexing arrays of TextMark data.
        You can use ItemSize() to find the size to move points on by and GetExtMarkInfo() where
        the rows is equivalent to the nMax parameter.

        SetTextMarkChan() is a special case of the command to create a TextMark channel with a
        single column.

        \sa SetTextMarkChan(), WriteExtMarks(), ReadExtMarks(), ItemSize(), GetExtMarkInfo(),
                  ChanDivide(), SetBuffering()
        \param chan  An unused channel to create as an extended marker channel.
        \param dRate The expected sustained maximum event rate.
        \param kind  This is one of: AdcMark (short items), RealMark (float items) or TextMark
                     (character items, see ).
        \param nRows The number of rows (at least 1) of attached items.
        \param nCols The number of columns (at least 1) of attached items.
        \param iPhyChan A physical source channel, used for information only.
        \param tDvd  If >0 this means that the attached data (taken by row) has a time axis.
                     This is usually used for AdcMark data, but it could also be used for RealMark
                     channels. The data in each row is assumed to start at the Marker time and be
                     spaced by lDvd. ChanDivide() returns this value. lDvd is in units of the file time base.
        \param  nPre This only has meaning when lDvd is > 0. It is used with AdcMark data, but could
                     also be used with RealMark data. It sets the index into the rows of data at which
                     the alignment point (often a peak) was located. if you set this negative you will not be
                     able to tell the difference between an error return to GetExtMarkInfo() or the correct
                     result. However, a negative value here might be useful/meaningful.
        \return S64_OK (0) or a negative error code.
        */
        virtual int SetExtMarkChan(TChanNum chan, double dRate, TDataKind kind, size_t nRows, size_t nCols = 1, int iPhyChan = -1, TSTime64 tDvd = 0, int nPre=0) = 0;

        //! Get extended marker data information
        /*!
        \ingroup GpExtMark
        Use this command to read by the rows, columns and number of pre-alignment points. Use
        ChanDivide() to read back the lDvd value set by SetExtMarkChan().

        \sa SetExtMarkChan(), ChanDivide()
        \param chan The channel number of an extended marker channel
        \param pRows Either nullptr or points at a variable to hold the number of rows
        \param pCols Either nullptr or points at a variable to hold the number of columns
        \return The nPre value set in the SetExtMarkChan() call or a negative error code (unless you have set a
                negative nPre value). If you already know that the channel exists and is of a sensible
                type, there can be no error.
        */
        virtual int GetExtMarkInfo(TChanNum chan, size_t *pRows = nullptr, size_t* pCols = nullptr) const = 0;

        //! Create a waveform channel using short or float data items
        /*!
        \ingroup GpWave
        The library embodies the concept of waveform channels. That is channels holding data that is equally spaced
        in time (but allowed to have gaps with missing data). Although we could allow data of any underlying type, the
        library is currently set for two types: short and float. These cover most of the target applications.

        The use of short data (16-bit signed integers) is convenient and compact (waveform data usually accounts for
        the bulk of the disk space used for data files. It also matches a common ADC specification. However, user
        data is more conveniently used in user units, so waveform channels have an associated scale and offset value
        that is used to convert between integer units and real units.

        \sa WriteWave(), ReadWave(), SetChanScale(), GetChanScale(), SetChanOffset(), GetChanOffset(),
                  SetTimeBase(), GetTimeBase(), SetBuffering(), IdealRate().
        \param chan An unused channel to be replaced by a waveform channel.
        \param lDvd The spacing of the channel in timebase units (SetTimeBase(), GetTimeBase()). This must be
                    greater than 0.
        \param wKind Either Adc or RealWave, the channel type.
        \param dRate The desired sample rate in Hz. It can happen that due to the choice of the file time base, the
                    lDvd value is only an approximation to the desired rate. This stores the desired rate for
                    information purposes, see IdealRate(). If set 0 or negative, dRate is calculated as:
                    1.0/(lDvd*GetTimeBase())
        \param iPhyCh The physical input channel associated with this, for information only.
        \return S64_OK (0) or a negative error code.
        */
        virtual int SetWaveChan(TChanNum chan, TSTime64 lDvd, TDataKind wKind, double dRate = 0.0, int iPhyCh=-1) = 0;

        //! Write waveform data as shorts to an Adc channel
        /*!
        \ingroup GpWave
        Unlike event-based channel types where all data must be written after any data already present in a channel,
        we allow you to overwrite previously-written Adc data. This is to remain compatible with the 32-bit SON library
        though it could be argued that the modification of primary data should not be allowed. You cannot fill in gaps
        in the orignal data in this manner. It is imagined that you would use this to fix glitches or remove transients.

        In normal use, all waveform data for a channel will be aligned at time that are the first data time plus an
        integer value times the lDvd for the channel. We do not prevent you writing data where the alignment is not
        the same after a gap, but programs like Spike2 will have subtle problems if you do this.

        \sa SetWaveChan(), ReadWave(), ChanDivide()
        \param chan The waveform channel to write to. This must be an Adc channel.
        \param pData The buffer of data to write.
        \param count The number of values to write.
        \param tFrom This is the time of the first item in the buffer to write. If this is after the last written data,
                     new values are appended to the file. If it is at or before the last time written, old data is
                     replaced.
        \return The next time to write to or a negative error code.
        */
        virtual TSTime64 WriteWave(TChanNum chan, const short* pData, size_t count, TSTime64 tFrom) = 0;

        //! Write waveform data as floats to a RealWave channel
        /*!
        \ingroup GpWave
        Unlike event-based channel types where all data must be written after any data already present in a channel,
        we allow you to overwrite previously-written RealWave data. This is to remain compatible with the 32-bit SON library
        though it could be argued that the modification of primary data should not be allowed. You cannot fill in gaps
        in the orignal data in this manner. It is imagined that you would use this to fix glitches or remove transients.

        In normal use, all waveform data for a channel will be aligned at time that are the first data time plus an
        integer value times the lDvd for the channel. We do not prevent you writing data where the alignment is not
        the same after a gap, but programs like Spike2 will have subtle problems if you do this.

        \sa SetWaveChan(), ReadWave(), ChanDivide()
        \param chan The waveform channel to write to. This must be a RealWave channel.
        \param pData The buffer of data to write.
        \param count The number of values to write.
        \param tFrom This is the time of the first item in the buffer to write. If this is after the last written data,
                     new values are appended to the file. If it is at or before the last time written, old data is
                     replaced.
        \return The next time to write to or a negative error code.
        */
        virtual TSTime64 WriteWave(TChanNum chan, const float* pData, size_t count, TSTime64 tFrom) = 0;

        //! Read an Adc, RealWave or an AdcMark channel as shorts
        /*!
        \ingroup GpWave
        If you read a RealWave channel, the values are converted to integers using the scale and offset set for the channel.
        The read operation stops at a gap in the data. A gap is defined as an interval between data points that is not
        the lDvd value defined for the channel.

        \sa SetWaveChan(), ReadWave(), GetChanScale(), GetChanOffset(), ChanDivide()
        \param chan The waveform channel to read. This can be either Adc or RealWave. If the type is RealWave we convert
                    values to short using the channel scale and offset. This may result in truncated values.
        \param pData The buffer to receive the read data.
        \param nMax  The maximum number of values to read.
        \param tFrom This and tUpto define a time range in which to locate the data to read. The first data point will
                     be at or after tFrom.
        \param tUpto Not data returned will be at or after this time.
        \param tFirst If any data is returned this is set to the time of the first item.
        \param pFilter Used with AdcMark channels to filter the data unless nulptr is passed.
        \return The number of values returned or a negative error code.
        */
        virtual int ReadWave(TChanNum chan, short* pData, int nMax, TSTime64 tFrom, TSTime64 tUpto, TSTime64& tFirst, const CSFilter* pFilter = nullptr) = 0;

        //! Read an Adc, RealWave or an AdcMark channel as floats
        /*!
        \ingroup GpWave
        If you read an Adc or AdcMark channel, the values are converted to floats using the scale and offset set for the channel.
        The read operation stops at a gap in the data. A gap is defined as an interval between data points that is not
        the lDvd value defined for the channel.

        \sa SetWaveChan(), ReadWave(), GetChanScale(), GetChanOffset(), ChanDivide()
        \param chan The waveform channel to read. This can be either Adc or RealWave. If the type is RealWave we convert
                    values to short using the channel scale and offset. This may result in truncated values.
        \param pData The buffer to receive the read data.
        \param nMax  The maximum number of values to read.
        \param tFrom This and tUpto define a time range in which to locate the data to read. The first data point will
                     be at or after tFrom.
        \param tUpto Not data returned will be at or after this time.
        \param tFirst If any data is returned this is set to the time of the first item.
        \param pFilter Used with AdcMark channels to filter the data unless nulptr is passed.
        \return The number of values returned or a negative error code.
        */
        virtual int ReadWave(TChanNum chan, float* pData, int nMax, TSTime64 tFrom, TSTime64 tUpto, TSTime64& tFirst, const CSFilter* pFilter = nullptr) = 0;

        //! Write extended marker data to a channel
        /*!
        \ingroup GpExtMark
        Extended marker data does not have a constant item size, so in the general case you will be using the
        ItemSize() function to iteraate through the data. In the 64-bit SON library, all extended marker types
        are a multiple of 8 bytes in size (to make sure that pointers are aligned). You will generally be
        casting the type of the data pointer to a TExtMarker* type. The written data MUST match the channel
        or the file will be corrupt.

        \sa SetExtMarkChan(), ReadExtMarks(), ItemSize(), GetExtMarkInfo()
        \param chan An extended marker channel.
        \param pData The buffer to write. The item size (how far to increment the pointer per item) depends on the
                     channel and we assume that you have got this correct.
        \param count The number of items to write.
        \return S64_OK (0) or a negative error code.
        */
        virtual int WriteExtMarks(TChanNum chan, const TExtMark* pData, size_t count) = 0;

        //! Read extended marker data from a channel
        /*!
        \ingroup GpExtMark
        Extended marker data does not have a constant item size, so in the general case you will be using the
        ItemSize() function to iteraate through the data. In the 64-bit SON library, all extended marker types
        are a multiple of 8 bytes in size (to make sure that pointers are aligned). You will generally be
        casting the type of the data pointer to a TExtMarker* type.

        Note that you can also read extended marker data as Markers with ReadMarkers() and as
        events with ReadEvents().

        \sa SetExtMarkChan(), WriteExtMarks(), ItemSize(), GetExtMarkInfo()
        \param chan An extended marker channel.
        \param pData The buffer to read. The item size (how far to increment the pointer per item) depends on the
                     channel , see ItemSize. You must get this correct to interpret the returned data.
        \param nMax The maximum number of items to read. The buffer should be at least nMax*ItemSize(chan) bytes.
        \param tFrom This and tUpto define a time range in which to locate the data to read. The first data point will
                     be at or after tFrom.
        \param tUpto Not data returned will be at or after this time.
        \param pFilter Either nulptr or a pointer to a filter used to select the items to read.
        \return      The number of items read or a negative error code.
        */
        virtual int ReadExtMarks(TChanNum chan, TExtMark* pData, int nMax, TSTime64 tFrom, TSTime64 tUpto, const CSFilter* pFilter = nullptr) = 0;
    };
}
#undef DllClass
#endif
