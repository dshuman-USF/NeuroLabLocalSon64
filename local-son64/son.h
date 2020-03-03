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
\brief Definitions of structures and routines usually needed to use Son32.

This is the include file for standard use of the 32-bit filing system. Note that
you can also access these files from the 64-bit system using a different API that
works for both 32-bit and 64-bit files (the Son64 system). We STRONGLY suggest that
all new programs use the Son64 API.

All access is by means of functions - no access to the internal Son32 data. The file
machine.h provides some common definitions across separate platforms. 

The library can write and read files up to 1 TB in size. Define NOBIGFILES to
remove big file support. If you do this, the library will not open or create big
files and the maximum file size will be 2 GB.
\#define NOBIGFILES.
*/

#ifndef __SON_H__
#define __SON_H__                   /*!< Guard macro for the son.h header*/

#include "machine.h"
#include <stdint.h>

/*!
S32I is defined to work around the fact that we used long in windows for years before
the arrival of 64-bit values. At this point, Unix/Linux define long as 64-bits and
leave int as 32, which causes Linux code to break, so we have a workaround.
*/
#define S32I int32_t

#ifdef _IS_WINDOWS_
#define SONAPI(type) type WINAPI
#undef S32I
/*! Windows defines this as a long for reasons of history */
#define S32I long
#endif

#ifdef LINUX
#define  SONAPI(type) type
#endif

enum
{
    SONMAXCHANS = 32,             /*!< The old limit on channels, now the minimum number */
    SONABSMAXCHANS = 451          /*!< Maximum possible channels from SON file structures */
};

/*!
Now define the error constants used throughout the program. The first lot are based
on the original error codes we got from MSDOS and the Mac!
*/
enum
{
    SON_NO_FILE = -1,              /*!< There is no file */
    SON_NO_DOS_FILE = -2,          /*!< Not used by son.c - historical */
    SON_NO_PATH = -3,              /*!< Not used by son.c - historical */
    SON_NO_HANDLES = -4,           /*!< Ran out of system disk handles */
    SON_NO_ACCESS = -5,            /*!< Access is not allowed */
    SON_BAD_HANDLE = -6,           /*!< A file handle is not legal */
    SON_MEMORY_ZAP = -7,           /*!< Not used by son.c - historical */
    SON_OUT_OF_MEMORY = -8,        /*!< Could not allocate memory */
    SON_INVALID_DRIVE = -15,       /*!< Not used by son.c - historical (Mac?) */
    SON_OUT_OF_HANDLES = -16,      /*!< This refers to SON file handles - too many files open */

    SON_FILE_ALREADY_OPEN = -600,  /*!< Used on 68k Mac, not used by son.c */

    SON_BAD_READ = -17,            /*!< A read operation failed */
    SON_BAD_WRITE = -18,           /*!< A erite operation failed */

    /*
    ** now some of our own errors, put in holes that we think we will never
    ** get from DOS...
    */
    SON_NO_CHANNEL = -9,           /*!< Referenced a channel that does not exist */
    SON_CHANNEL_USED = -10,        /*!< The channel is already in used */
    SON_CHANNEL_UNUSED = -11,      /*!< The channel is valid but is unused */
    SON_PAST_EOF = -12,            /*!< Disk file operation past the end of file */
    SON_WRONG_FILE = -13,          /*!< Attempt to open a file that is not Son32 */
    SON_NO_EXTRA = -14,            /*!< No extra data space is defined for the file */
    SON_CORRUPT_FILE = -19,        /*!< The file is internally inconsistent */
    SON_PAST_SOF = -20,            /*!< Attempt to access a file at a negative offset */
    SON_READ_ONLY = -21,           /*!< The file is read only, cannot be modified */
    SON_BAD_PARAM = -22,           /*!< Argument to a command is illegal */
    SON_DLL64 = -23,               /*!< internal: the handle refers to a 64-bit file */
};

/*!
These constants define the number and length of various strings. Characters counts
are byte counts (the original strings were ASCII only).
*/
enum
{
    SON_NUMFILECOMMENTS = 5,       /*!< Number of File comments, first is index 0 */
    SON_COMMENTSZ = 79,            /*!< Maximum characters in a file comment */
    SON_CHANCOMSZ = 71,            /*!< Maximum characters in a channel comment */
    SON_UNITSZ = 5,                /*!< Maximum characters in a channel units string */
    SON_TITLESZ = 9                /*!< Maximum characters in a channel title string*/
};

/*!
** These define the types of data we can store in our file, and a type
** that will hold one of these values.
*/
typedef enum
{
    ChanOff=0,          /*!< the channel is OFF - */
    Adc,                /*!< a 16-bit waveform channel */
    EventFall,          /*!< Event times (falling edges) */
    EventRise,          /*!< Event times (rising edges) */
    EventBoth,          /*!< Event times (both edges) */
    Marker,             /*!< Event time plus 4 8-bit codes */
    AdcMark,            /*!< Marker plus Adc waveform data */
    RealMark,           /*!< Marker plus float numbers */
    TextMark,           /*!< Marker plus text string */
    RealWave            /*!< waveform of float numbers */
} TDataKind;

/*!
** These constants defines the state of a created file. They should never be
** used in modern code.
*/
enum
{
    FastWrite = 0,      /*!< A file which needs the forward links filling in*/
    NormalWrite = 1     /*!< A file with the links filled in */
};

typedef int32_t TSTime;         /*!< The basic 32-bit time type*/
enum
{
    TSTIME_MAX = INT32_MAX    /*!< The maximum time value*/
};

typedef int16_t TAdc;           /*!< The 16-bit Adc data type*/
typedef char TMarkBytes[4];     /*!< A type for the 4 marker codes */

/*!
**  The TMarker structure defines the marker data structure, which holds
**  a time value with associated data. The TAdcMark structure is a marker
**  with attached array of ADC data. TRealMark and TTextMark are very
**  similar - with real or text data attached.
*/
typedef struct
{
    TSTime mark;                /*!< Marker time as for events */
    TMarkBytes mvals;           /*!< the marker values */
} TMarker;                      /*!< Type to define a marker*/

enum
{
    SON_MAXADCMARK = 1024,     /*!< maximum points in AdcMark data (arbitrary) */
    SON_MAXAMTRACE = 4,        /*!< maximum interleaved traces in AdcMark data */
    SON_MAXREALMARK = 512,     /*!< maximum points in RealMark (arbitrary) */
    SON_MAXTEXTMARK = 2048,    /*!< maximum points in a Textmark (arbitrary) */
};

/*! \brief Structure to express the concept of an AdcMark data item.

This is a dynamically sized item. It is up to the programmer to deal with the
size of an item belonging to a data channel. The actual size of the TAdcMark
for a particular channel is available through the SONItemSize() command.
*/
typedef struct
{
    TMarker m;                  /*!< the marker structure */
    TAdc a[SON_MAXADCMARK*SON_MAXAMTRACE];     /*!< the attached ADC data */
} TAdcMark;                     /*!< Type to define AdcMark data items */


/*! \brief Structure to express the concept of a RealMark item.

This is a dynamically sized item. It is up to the programmer to deal with the
size of an item belonging to a data channel. The actual size of the TRealMark
for a particular channel is available through the SONItemSize() command.
*/
typedef struct
{
    TMarker m;                  /*!< the marker structure */
    float r[SON_MAXREALMARK];   /*!< the attached floating point data */
} TRealMark;                    /*!< Type to define a RealMark item */


/*! \brief Structure to express the concept of a textMark item.

This is a dynamically sized item. It is up to the programmer to deal with the
size of an item belonging to a data channel. The actual size of the TTextMark
for a particular channel is available through the SONItemSize() command.
*/
typedef struct
{
    TMarker m;                  /*!< the marker structure */
    char t[SON_MAXTEXTMARK];    /*!< the attached text data */
} TTextMark;                    /*!< Type to define a TextMark item */

typedef TAdc FAR * TpAdc;           /*!< Pointer to 16-bit Adc data */
typedef TSTime FAR *TpSTime;        /*!< Pointer to* TSTime times */
typedef TMarker FAR * TpMarker;     /*!< Pointer to Marker data items */
typedef TAdcMark FAR * TpAdcMark;   /*!< Pointer to AdcMark data items*/
typedef TRealMark FAR * TpRealMark; /*!< Pointer to RealMark data items*/
typedef TTextMark FAR * TpTextMark; /*!< Pointer to TextMark data items*/
typedef char FAR * TpStr;           /*!< Points at text strings */
typedef const char FAR * TpCStr;    /*!< Points at const text strings */
typedef WORD FAR * TpWORD;          /*!< pointer to WORD data */
typedef BOOLEAN FAR * TpBOOL;       /*!< Pointer to BOOLEAN data */
typedef float FAR * TpFloat;        /*!< Pointer to float data */
typedef void FAR * TpVoid;          /*!< POinter to void data */
typedef short FAR * TpShort;        /*!< pointer to short data == TpAdc */
typedef TMarkBytes FAR * TpMarkBytes; /*!< Pointer to TMarkBytes */

#ifdef _UNICODE
typedef const wchar_t* TpCFName;    /*!< Pointer to file name */
#else
typedef const char* TpCFName;       /*!< Pointer to file name */
#endif

enum
{
    SON_FMASKSZ = 32,               /*!< number of TFilterElt in mask */
    SON_FMASK_ORMODE = 0x02000000,  /*!< use OR if data rather than AND */
    SON_FMASK_ANDMODE = 0x00000000, /*!< use AND mode */
    SON_FMASK_VALID = 0x02000000,   /*!< bits that are valid in the mask */
};
typedef unsigned char TFilterElt;           /*!< element of a map */
typedef TFilterElt TLayerMask[SON_FMASKSZ]; /*!< 256 bits in the bitmap */

/*! \brief A filter mask to apply to marker and extended marker channels.

We have changed the implementation of TFilterMask so we can use the
marker filer in different ways. As long as you made no use of cAllSet
the changes should be transparent as the structure is the same size.
if you MUST have the old format, \#define SON_USEOLDFILTERMASK

In the new method we no longer use flags to show that an entire layer is
set. Instead, we have a filter mode (bit 0 of the lFlags). All other bits
should be 0 as we will use them for further modes in future and we intend
this to be backwards compatible.

We also have defined SONFMode() to get/set the filter mode. We avoid bits
0, 8, 16 and 24 as these were used in the old version to flag complete masks.
*/
typedef struct
{
#ifdef SON_USEOLDFILTERMASK
    char cAllSet[4];                        /*!< set non-zero if all markers enabled */
#else
    int32_t lFlags;                         /*!< private flags used by the marker filering */
#endif
    TLayerMask aMask[4];                    /*!< set of masks for each layer */
    int32_t nTrace;                         /*!< Trace select for reading AdcMark as waveform */
} TFilterMask;                              /*!< Type to define a filter mask*/

typedef TFilterMask FAR *TpFilterMask;      /*!< Pointer to a TFilterMask item */

/*!
Constants used with FilterMask items to select items, layers and actions.
*/
enum
{
    SON_FALLLAYERS = -1,    /*!< Select all layers */
    SON_FALLITEMS = -1,     /*!< Select all items in all layers */
    SON_FCLEAR = 0,         /*!< Clear the selected items */
    SON_FSET = 1,           /*!< Set the selected items*/
    SON_FINVERT = 2,        /*!< Invert the state of the selected items */
    SON_FREAD = -1,         /*!< Code to read back a layer */
};

#ifdef __cplusplus
extern "C" {
#endif


/*
** Now definitions of the functions defined in the code
*/
SONAPI(void) SONInitFiles(void);
SONAPI(void) SONCleanUp(void);

SONAPI(short) SONOpenOldFile(TpCFName name, int iOpenMode);
SONAPI(short) SONOpenNewFile(TpCFName name, short fMode, WORD extra);

SONAPI(BOOLEAN) SONCanWrite(short fh);
SONAPI(short) SONCloseFile(short fh);
SONAPI(short) SONEmptyFile(short fh);
SONAPI(short) SONSetBuffSpace(short fh);
SONAPI(short) SONGetFreeChan(short fh);
SONAPI(void) SONSetFileClock(short fh, WORD usPerTime, WORD timePerADC);
SONAPI(short) SONSetADCChan(short fh, WORD chan, short sPhyCh, short dvd,
                 S32I lBufSz, TpCStr szCom, TpCStr szTitle, float fRate,
                 float scl, float offs, TpCStr szUnt);
SONAPI(short) SONSetADCMarkChan(short fh, WORD chan, short sPhyCh, short dvd,
                 S32I lBufSz, TpCStr szCom, TpCStr szTitle, float fRate, float scl,
                 float offs, TpCStr szUnt, WORD points, short preTrig);
SONAPI(short) SONSetWaveChan(short fh, WORD chan, short sPhyCh, TSTime dvd,
                 S32I lBufSz, TpCStr szCom, TpCStr szTitle,
                 float scl, float offs, TpCStr szUnt);
SONAPI(short) SONSetWaveMarkChan(short fh, WORD chan, short sPhyCh, TSTime dvd,
                 S32I lBufSz, TpCStr szCom, TpCStr szTitle, float fRate, float scl,
                 float offs, TpCStr szUnt, WORD points, short preTrig, int nTrace);
SONAPI(short) SONSetRealMarkChan(short fh, WORD chan, short sPhyCh,
                 S32I lBufSz, TpCStr szCom, TpCStr szTitle, float fRate,
                 float min, float max, TpCStr szUnt, WORD points);
SONAPI(short) SONSetTextMarkChan(short fh, WORD chan, short sPhyCh,
                 S32I lBufSz, TpCStr szCom, TpCStr szTitle,
                 float fRate, TpCStr szUnt, WORD points);
SONAPI(void) SONSetInitLow(short fh, WORD chan, BOOLEAN bLow);
SONAPI(short) SONSetEventChan(short fh, WORD chan, short sPhyCh,
                 S32I lBufSz, TpCStr szCom, TpCStr szTitle, float fRate, TDataKind evtKind);

SONAPI(short) SONSetBuffering(short fh, int nChan, int nBytes);
SONAPI(short) SONUpdateStart(short fh);
SONAPI(void) SONSetFileComment(short fh, WORD which, TpCStr szFCom);
SONAPI(void) SONGetFileComment(short fh, WORD which, TpStr pcFCom, short sMax);
SONAPI(void) SONSetChanComment(short fh, WORD chan, TpCStr szCom);
SONAPI(void) SONGetChanComment(short fh, WORD chan, TpStr pcCom, short sMax);
SONAPI(void) SONSetChanTitle(short fh, WORD chan, TpCStr szTitle);
SONAPI(void) SONGetChanTitle(short fh, WORD chan, TpStr pcTitle);
SONAPI(void) SONGetIdealLimits(short fh, WORD chan, TpFloat pfRate, TpFloat pfMin, TpFloat pfMax);
SONAPI(WORD) SONGetusPerTime(short fh);
SONAPI(WORD) SONGetTimePerADC(short fh);
SONAPI(void) SONSetADCUnits(short fh, WORD chan, TpCStr szUnt);
SONAPI(void) SONSetADCOffset(short fh, WORD chan, float offset);
SONAPI(void) SONSetADCScale(short fh, WORD chan, float scale);
SONAPI(void) SONGetADCInfo(short fh, WORD chan, TpFloat scale, TpFloat offset,
                 TpStr pcUnt, TpWORD points, TpShort preTrig);
SONAPI(void) SONGetExtMarkInfo(short fh, WORD chan, TpStr pcUnt,
                 TpWORD points, TpShort preTrig);

SONAPI(short) SONWriteEventBlock(short fh, WORD chan, TpSTime plBuf, S32I count);
SONAPI(short) SONWriteMarkBlock(short fh, WORD chan, TpMarker pM, S32I count);
SONAPI(TSTime) SONWriteADCBlock(short fh, WORD chan, TpAdc psBuf, S32I count, TSTime sTime);
SONAPI(short) SONWriteExtMarkBlock(short fh, WORD chan, TpMarker pM, S32I count);

SONAPI(short) SONSave(short fh, int nChan, TSTime sTime, BOOLEAN bKeep);
SONAPI(short) SONSaveRange(short fh, int nChan, TSTime sTime, TSTime eTime);
SONAPI(short) SONKillRange(short fh, int nChan, TSTime sTime, TSTime eTime);
SONAPI(short) SONIsSaving(short fh, int nChan);
SONAPI(DWORD) SONFileBytes(short fh);
SONAPI(DWORD) SONChanBytes(short fh, WORD chan);

SONAPI(short) SONLatestTime(short fh, WORD chan, TSTime sTime);
SONAPI(short) SONCommitIdle(short fh);
SONAPI(short) SONCommitFile(short fh, BOOLEAN bDelete);

//! Flag values for SONCommitFileEx()
enum
{
    SON_CF_FLUSHSYS = 1,       /*!< Flush operating system buffers. Can be _very_ slow.*/
    SON_CF_HEADERONLY = 2,     /*!< Write file header only, not write buffers. */
    SON_CF_DELBUFFER = 4,      /*!< Delete write buffers after flush. */
};
SONAPI(short) SONCommitFileEx(short fh, int flags); // bit 0 = flush, bit 1=header only, bit 2=delete buffer

SONAPI(S32I) SONGetEventData(short fh, WORD chan, TpSTime plTimes, S32I max,
                  TSTime sTime, TSTime eTime, TpBOOL levLowP, 
                  TpFilterMask pFiltMask);
SONAPI(S32I) SONGetMarkData(short fh, WORD chan,TpMarker pMark, S32I max,
                  TSTime sTime,TSTime eTime, TpFilterMask pFiltMask);
SONAPI(S32I) SONGetADCData(short fh,WORD chan,TpAdc adcDataP, S32I max,
                  TSTime sTime,TSTime eTime,TpSTime pbTime,
                  TpFilterMask pFiltMask);

SONAPI(S32I) SONGetExtMarkData(short fh, WORD chan, TpMarker pMark, S32I max,
                  TSTime sTime,TSTime eTime, TpFilterMask pFiltMask);
SONAPI(S32I) SONGetExtraDataSize(short fh);
SONAPI(int) SONGetVersion(short fh);
SONAPI(short) SONGetExtraData(short fh, TpVoid buff, WORD bytes,
                  WORD offset, BOOLEAN writeIt);
SONAPI(short) SONSetMarker(short fh, WORD chan, TSTime time, TpMarker pMark,
                  WORD size);
SONAPI(short)  SONChanDelete(short fh, WORD chan);
SONAPI(TDataKind) SONChanKind(short fh, WORD chan);
SONAPI(TSTime) SONChanDivide(short fh, WORD chan);
SONAPI(WORD)   SONItemSize(short fh, WORD chan);
SONAPI(TSTime) SONChanMaxTime(short fh, WORD chan);
SONAPI(TSTime) SONMaxTime(short fh);

SONAPI(TSTime) SONLastTime(short fh, WORD wChan, TSTime sTime, TSTime eTime,
                    TpVoid pvVal, TpMarkBytes pMB,
                    TpBOOL pbMk, TpFilterMask pFiltMask);

SONAPI(TSTime) SONLastPointsTime(short fh, WORD wChan, TSTime sTime, TSTime eTime,
                    S32I lPoints, BOOLEAN bAdc, TpFilterMask pFiltMask);

SONAPI(S32I) SONFileSize(short fh);
SONAPI(int) SONMarkerItem(short fh, WORD wChan, TpMarker pBuff, int n,
                                          TpMarker pM, TpVoid pvData, BOOLEAN bSet);

SONAPI(int) SONFilter(TpMarker pM, TpFilterMask pFM);
SONAPI(int) SONFControl(TpFilterMask pFM, int layer, int item, int set);
SONAPI(BOOLEAN) SONFEqual(TpFilterMask pFiltMask1, TpFilterMask pFiltMask2);
SONAPI(int) SONFActive(TpFilterMask pFM);   // added 14/May/02

#ifndef SON_USEOLDFILTERMASK
SONAPI(S32I) SONFMode(TpFilterMask pFM, S32I lNew);
#endif

/****************************************************************************
** New things added at Revision 6
*/

/*! \brief Structure to define a time and date.

This has the same internal structure as the \ref Son64 structure called
ceds64::TTimeDate
*/
typedef struct
{
    unsigned char ucHun;    /*!< hundreths of a second, 0-99 */
    unsigned char ucSec;    /*!< seconds, 0-59 */
    unsigned char ucMin;    /*!< minutes, 0-59 */
    unsigned char ucHour;   /*!< hour - 24 hour clock, 0-23 */
    unsigned char ucDay;    /*!< day of month, 1-31 */
    unsigned char ucMon;    /*!< month of year, 1-12 */
    WORD wYear;             /*!< year 1980-65535! */
}TSONTimeDate;              /*!< Structure to hold the time and date sampling started */

SONAPI(short) SONCreateFile(TpCFName name, int nChannels, WORD extra);
SONAPI(int) SONMaxChans(short fh);
SONAPI(int) SONPhyChan(short fh, WORD wChan);
SONAPI(float) SONIdealRate(short fh, WORD wChan, float fIR);
SONAPI(void) SONYRange(short fh, WORD chan, TpFloat pfMin, TpFloat pfMax);
SONAPI(int) SONYRangeSet(short fh, WORD chan, float fMin, float fMax);
SONAPI(int) SONMaxItems(short fh, WORD chan);
SONAPI(int) SONPhySz(short fh, WORD chan);
SONAPI(int) SONBlocks(short fh, WORD chan);
SONAPI(int) SONDelBlocks(short fh, WORD chan);
SONAPI(int) SONSetRealChan(short fh, WORD chan, short sPhyChan, TSTime dvd,
                 S32I lBufSz, TpCStr szCom, TpCStr szTitle,
                 float scale, float offset, TpCStr szUnt);
SONAPI(TSTime) SONWriteRealBlock(short fh, WORD chan, TpFloat pfBuff, S32I count, TSTime sTime);
SONAPI(S32I) SONGetRealData(short fh, WORD chan, TpFloat pfData, S32I max,
                  TSTime sTime,TSTime eTime,TpSTime pbTime, 
                  TpFilterMask pFiltMask);
SONAPI(int) SONTimeDate(short fh, TSONTimeDate* pTDGet, const TSONTimeDate* pTDSet);
SONAPI(double) SONTimeBase(short fh, double dTB);

/*! \brief Structure used to hold the application identifier.

This has the same internal structure as the \ref Son64 structure defined by
ceds64::TCreator
*/
typedef struct
{
    char acID[8];   /*!< Space for 8 bytes of application identifier (usually zero padded text) */
} TSONCreator;      /*!< Application identifier */
SONAPI(int) SONAppID(short fh, TSONCreator* pCGet, const TSONCreator* pCSet);
SONAPI(int) SONChanInterleave(short fh, WORD chan);

/* Version 7 */
SONAPI(int) SONExtMarkAlign(short fh, int n);

/* Version 9 */
SONAPI(double) SONFileSizeD(short fh);
SONAPI(double) SONChanBytesD(short fh, WORD chan);
SONAPI(short) SONCreateFileEx(TpCFName name, int nChannels, WORD extra, int iBigFile);
SONAPI(int) SONIsBigFile(short fh);
SONAPI(int) SONUpdateHeader(short fh);
#ifdef __cplusplus
}
#endif

#endif /* __SON__ */
