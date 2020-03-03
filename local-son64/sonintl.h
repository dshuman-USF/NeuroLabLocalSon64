/*
    Copyright (C) Cambridge Electronic Design Limited 1988, 1990-1995, 1998-2010, 2012-2103 
    Author: Greg P. Smith
    Web: www.ced.co.uk email: greg@ced.co.uk

    This file is part of SON32, the 32-bit SON data library. It is also included in SON64,
    the 64-bit SON data library to allow it to read 32-bit files.

    SON32 is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    SON32 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with SON32.  If not, see <http://www.gnu.org/licenses/>.
*/

/*!
\file
\brief File structure definitions and internals not needed for general Son32 use
\internal

Definitions of the file structures and other internals for the SON filing system.
This is not normally required by library users, but is supplied for specialised use.
The file machine.h (included in son.h) provides some common definitions across
separate platforms.

This should be included AFTER son.h.
*/

#ifndef __SONINTL__
#define __SONINTL__
#include "son.h"

#define LSTRING(size) union{unsigned char len; char string[size+1];}

/*!< the current version of the library */
#define SON_VER 9 
/*!< the last version with small files */
#define SON_VERSMALL 8

#ifdef _IS_WINDOWS_
/*!< The maximum number of files allowed */
#define MAXFILES 256
#else
/*!< The maximum number of files allowed */
#define MAXFILES 32
#endif

/*!< Initial lookup table entries per channel */
#define MINLOOK 32
/*!< Lookup table entries per channel (must be Power of 2 */
#define MAXLOOK 2048
/*!< Write buffers per channel in new file */
#define MAXWBUF 128
/*!< Stored changes per channel in new file */
#define CHANGES 8
/*!< Size of a disk block */
#define DISKBLOCK 512
/*!< 1 << DISKBLOCK_SHIFT is DISKBLOCK */
#define DISKBLOCK_SHIFT 9
/*!< Macro to round a size up to a disk block*/
#define ROUND_TO_DB(num) (((num)+DISKBLOCK-1)&0xfe00)
#define LENCOPYRIGHT  10                    /*!< Length of copyright and serial strings */
/*!< The copyright string used */
#define COPYRIGHT "(C) CED 87"

/*
** the field osFormat is a 16 bit word, used to detect file format, so byte
** swapping will not affect this value
*/
#define DOSFORMAT   0x0000
#define MACFORMAT   0x0101
#define OSFORMAT DOSFORMAT

/*!< The value that indicates the end of chain of blocks */
#define CHAINEND ((TDOF)-1)

typedef LSTRING(SON_CHANCOMSZ) TChanComm;   /*!< Channel comment definition */
typedef LSTRING(SON_COMMENTSZ) TComment;    /*!< File comment definition */
typedef LSTRING(SON_TITLESZ) TTitle;        /*!< Channel Title definition */
typedef TComment TFileComment[SON_NUMFILECOMMENTS];  /*!< Block of file comments for header */
typedef LSTRING(SON_UNITSZ) TUnits;         /*!< Channel units string (used by wave-related) */

/*! Macro to move a generic pointer on by n bytes */
#define movePtr(p,n) ((void FAR *)((TpStr)(p) + (n)))

/*!
A version 9 file stores disk offsets as block numbers; each block being
DISKBLOCK (512) bytes. This increases the maximum file size to 1 TB, a
size that would take a really long time to find anything in! We have
changed the type of all file offsets from long to TDOF (Type Disk OFfset).

TDOF values in the program are used for access to data. The value 0 is not
valid as this is where the file header lives; 0 is used for block not found
when searching. Values from 0x80000000 to 0x8000007f are used to indicate
the channel buffers 0 to 0x7f. Other negative values are error codes.

If the disk version of a file is 9 or more, TDOF values are blocks. For
earlier file versions, TDOF values are bytes and the maximum file size is
2 GB.

To implement 64 bit support on systems other than windows, you must define
a suitable 64-bit type for TDOF64 and then implement the following routines
for your operating system:
SONWrite64(), SONRead64(), SysFileSize()

You must also adjust the following static routines:
To64(), TDOF64ToTDOF(),

The implementation for Windows uses the standard LARGE_INTEGER type that is
used in the Windows API for file access.
*/
typedef int32_t TDOF;               /*!< disk offset type for Son32 files */

#ifdef NOBIGFILES
typedef int32_t TDOF64;             /*!< Disk offset type when no big files */
  #ifdef LINUX
  #define open open64
  #define lseek lseek64
  #define ftruncate ftruncate64
  #endif
#else
  #ifdef _IS_WINDOWS_
    typedef LARGE_INTEGER TDOF64;   /*!< 64-bit disk offset type for big files */
  #elif defined(LINUX)
    typedef off64_t TDOF64;         /*!< 64-bit disk offset type for big files */
  #else
    typedef long long TDOF64;       /*!< 64-bit disk offset type for big files */
  #endif
#endif

#if defined(_IS_WINDOWS_) || defined(LINUX)
#pragma pack(2)
#endif

/*!
\brief Structure that defines the first block of a Son32 file on disk.

We pad the structure out to 512 bytes as we assume that all files are efficient
if we make reads and writes multiples of 512 (this was a common sector size when
we started writing this library - many years ago)
*/
typedef
struct TFileHead
{
    short systemID;               /*!< filing system revision level */
    char copyright[LENCOPYRIGHT]; /*!< space for "(C) CED 87" */
    TSONCreator creator;          /*!< optional application identifier */
    WORD usPerTime;               /*!< microsecs per time unit */
    WORD timePerADC;              /*!< time units per ADC interrupt */
    short fileState;              /*!< condition of the file */
    TDOF firstData;               /*!< offset to first data block */
    short channels;               /*!< maximum number of channels */
    WORD chanSize;                /*!< memory size to hold chans */
    WORD extraData;               /*!< No of bytes of extra data in file */
    WORD bufferSz;                /*!< Not used on disk; bufferP in bytes */
    WORD osFormat;                /*!< either 0x0101 for Mac, or 0x00 for PC */
    TSTime maxFTime;              /*!< max time in the data file */
    double dTimeBase;             /*!< time scale factor, normally 1.0e-6 */
    TSONTimeDate timeDate;        /*!< time that corresponds to tick 0 */ 
    char cAlignFlag;              /*!< 0 if not aligned to 4, set bit 1 if aligned */
    char pad0[3];                 /*!< align the next item to a 4 byte boundary */
    TDOF LUTable;                 /*!< 0, or the TDOF to a saved look up table on disk */
    char pad[44];                 /*!< padding for the future */
    TFileComment fileComment;     /*!< what user thinks of it so far */
} TFileHead;

typedef struct TFileHead FAR * TpFileHead; /*!< Pointer to the file header in memory */

/*!
\brief Structure defining one data channel that is stored on disk.

A contiguous array of these follows the header block of the file. The size of
this is critical; various elements have been reused/repurposed in different
versions of the Son32 file system.
*/
typedef
struct TChannel
{
    WORD delSize;       /*!< number of blocks in deleted chain, 0=none */
    TDOF nextDelBlock;  /*!< if deleted, first block in chain pointer */
    TDOF firstBlock;    /*!< points at first block in file */
    TDOF lastBlock;     /*!< points at last block in file */
    WORD blocks;        /*!< number of blocks in file holding data */
    WORD nExtra;        /*!< Number of extra bytes attached to marker */
    short preTrig;      /*!< Pre-trig points for ADC Marker data */
    short blocksMSW;    /*!< hi word of block count in version 9 */
    WORD phySz;         /*!< physical size of block written =n*512 */
    WORD maxData;       /*!< maximum number of data items in block */
    TChanComm comment;  /*!< string commenting on this data */
    TSTime maxChanTime; /*!< last time on this channel */
    TSTime lChanDvd;    /*!< Was 0, V6: waveform divide from usPerTime, else 0 */
    short phyChan;      /*!< physical channel used */
    TTitle title;       /*!< user name for channel */
    float idealRate;    /*!< ideal rate:ADC, estimate:event */
    unsigned char kind; /*!< data type in the channel - really is TDataKind */
    unsigned char delSizeMSB;   /*!< extension for deleted chain size if version 9 */

    union                       /*!< Section which changes with the data */
    {
        struct                  /*!< For ADC and ADCMark channels */
        {
            float scale;        /*!< Channel scaling factor for 16-bit to real */
            float offset;       /*!< Channel offset for 16-bit to real */
            TUnits units;       /*!< channel units */
            WORD divide;        /*!< was ADC divide, now AdcMark interleave */
        } adc;
        struct                  /*!< For EventBoth channels */
        {     
            BOOLEAN initLow;    /*!< initial event state */
            BOOLEAN nextLow;    /*!< expected state of next write */
        } event;
        struct                  /*!< For real marker data */
        {     
            float min;          /*!< expected minimum value */
            float max;          /*!< expected maximum value */
            TUnits units;       /*!< channel units */
        } real;                 /*!< NB this is laid out as for adc data */
    } v;                        /*!< This is the union of various types; v is for Varies */

} TChannel;

typedef struct TChannel FAR * TpChannel;   /*!< Pointer to a channel object in memory */

/*! \brief Macro to calculate the size of the channels array.

The channels array is saved on disk starting at offset 512 into the file.
CHANSIZE is a Macro for the size of this structure, rounded up to a disk block.
Apologies for the 0xfe00, but this is simplest way to express the result.
*/
#define CHANSIZE(n) ((sizeof(TChannel)*n + DISKBLOCK-1) & 0xFE00)

/*
** These two macros convert channel numbers between the (rather nasty)
**  arrangement we are forced to use on disk and simple numbers used
**  internally. On disk channel numbers are 9 bits. We store the channel
**  number+1, with the bottom 8 bits in the 8 bits of the data and the
**  ninth bit stored in the tenth bit of the data.
*/
#define CHFRDISK(n) (((n & 0xff) + ((n & 0x200) >> 1))-1)
#define CHTODISK(n) (((n+1) & 0xff) + (((n+1) & 0x100) << 1))
#define CHLVLBIT 0x100

enum
{
    ADCdataBlkSize = 32000,     /*!< Arbitrary items in waveform block */
    timeDataBlkSize = 16000,    /*!< Arbitrary items in event time block */
    markDataBlkSize = 8000,     /*!< Arbitrary items in Marker block */
    realDataBlkSize = 8000,     /*!< Arbitrary items in RealWave block*/
};

/*!
\brief Structure used to access the Son32 data blocks

The data is stored in blocks (again multiples of 512 bytes long) on disk.
All the blocks have an identical header, but the rest depends on the data type.
*/
typedef
struct TDataBlock
{
    TDOF   predBlock;     /*!< Predecessor block in the file */
    TDOF   succBlock;     /*!< Following block in the file */
    TSTime startTime;     /*!< First time in the block */
    TSTime endTime;       /*!< Last time in the block */
    WORD   chanNumber;    /*!< Channel number+1 for the block */
    WORD   items;         /*!< Actual number of data items found */
    union                 /*! Union of the various types of data after the header */
    {
        TAdc      int2Data [ADCdataBlkSize];    /*!< ADC data */
        TSTime    int4Data [timeDataBlkSize];   /*!< time data */
        TMarker   markData [markDataBlkSize];   /*!< marker data */
        TAdcMark  adcMarkData;                  /*!< ADC marker data */
        float     realData [realDataBlkSize];   /*!< RealWave data */
    } data;               /*!< The data area of the TDataBlock, a union of various types*/
} TDataBlock;

typedef struct TDataBlock FAR * TpDataBlock; /*!< Pointer to TDataBlock */

/*!< size of the TDataBlock header itself */
#define SONDBHEADSZ 20

extern TpDataBlock workP;           /*!< points at DISKBLOCK bytes work area */

#if defined(_IS_WINDOWS_) || defined(LINUX)
#pragma pack()
#endif


/*
** Define an internal type TFH here to stand in for the various
** file handle types used to make things
*/
#ifdef _IS_WINDOWS_
#define TFH HANDLE
#endif
#ifdef LINUX
#define  TFH int
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
** Now declarations of the functions defined in the code
*/
SONAPI(short)  SONRead64(short fh, void FAR * buffer, DWORD bytes, TDOF64 offset);
SONAPI(short)  SONWrite64(short fh, void FAR * buffer, DWORD bytes, TDOF64 offset);
SONAPI(short)  SONRead(short fh, void FAR * buffer, WORD bytes, TDOF offset);
SONAPI(short)  SONWrite(short fh, void FAR * buffer, WORD bytes, TDOF offset);
SONAPI(short)  SONGetBlock(short fh, TDOF offset);
SONAPI(TDOF)   SONGetPred(short fh, TDOF offset) ;
SONAPI(TDOF)   SONGetSucc(short fh, TDOF offset);
SONAPI(short)  SONSetSucc(short fh, TDOF offset, TDOF succOffs);
SONAPI(TDOF)   SONFindBlock(short fh, WORD channel,TSTime sTime, TSTime eTime);
SONAPI(short)  SONReadBlock(short fh, WORD channel, TDOF position);
SONAPI(short)  SONWriteBlock(short fh, WORD chan, BYTE* buffer, S32I items,
                            int nSize, TSTime sTime, TSTime eTime);
SONAPI(TpChannel) SONChanPnt(short fh, WORD chan);
SONAPI(TSTime) SONIntlChanMaxTime(short fh, WORD chan);
SONAPI(TSTime) SONIntlMaxTime(short fh);
SONAPI(S32I)   SONUpdateMaxTimes(short fh);
SONAPI(void)   SONExtendMaxTime(short fh, TSTime time);
SONAPI(S32I)   SONGetFirstData(short fh);
SONAPI(TFH)    SONFileHandle(short fh);
SONAPI(short)  SONBookFileSpace(short fh, S32I lSpace);

/* from here on, functions that are new in VERSION 6 */

SONAPI(void)   SONSetPhySz(short fh, WORD chan, S32I lSize);

/*
** These two functions are intended to be used to find out about channel data
**  without reading it all. SONNextSection is used to find contiguous sections
**  of waveform data, SONItemCount returns the count of data items, primarily
**  for event type channels.
*/
//SONAPI(short)  SONNextSection(short fh, WORD chan, TSTime& sTime, TSTime& eTime);
//SONAPI(S32I)   SONItemCount(short fh, WORD chan);

#ifdef __cplusplus
}
#endif

#endif
