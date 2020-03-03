#ifndef __SONPRIV_H__
#define __SONPRIV_H__
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
\brief Private structures used in memory to track Son32 file operations.

The structures in this file do not live on disk, but are created by the SON
library to hold internal information as required. TLookup is used to optimise
and speed-up searching for data on disk, TWrBuf is a write buffer used to
hold data before it is written to a file and TChange is used to store
information about a save/discard change. The TChInfo structure holds all of
these, plus other information, and is the master location of all non-disk
based channel information particularly as regards writing data.

\def MTHREAD
If the MTHREAD macro is defined, code is generated to protect Son32 file and
channel data structures from simultaneous access on multiple threads. In the
\ref Son32 library this protection is not as fine grained and sophisticated as
in the \ref Son64 library. It is designed for the case where one thread is
writing data and another is reading it. It is assumed that only one thread
creates and deletes channels. Currently, MTHREAD is always defined when
building with `_IS_WINDOWS_` defined. It coould be implemented for other systems
by defining the *`_CRIT_`* macros for other systems.

\def INIT_CRIT_CHAN(fhxx, chanxx)
If `_IS_WINDOWS_` is defined, this expands to code that initialises a channel
mutex to protect against multiple simultaneous use. `fhxx` is the file handle
and `chanxx`is the channel number.

\def DEL_CRIT_CHAN(fhxx, chanxx)
if `_IS_WINDOWS_` is defined, this expands to code that delets a channel mutex.
`fhxx` is the file handle and `chanxx`is the channel number.

\def START_CRIT_CHAN(fhxx, chanxx)
If `_IS_WINDOWS_` is defined, this expands to code that attempts to acquire a 
channel mutex before using channel data to prevent simultaneous modifications.
`fhxx` is the file handle and `chanxx`is the channel number.

\def END_CRIT_CHAN(fhxx, chanxx)
If `_IS_WINDOWS_` is defined, this expands to code to release a channel mutex
after using the channel data to allow other threads to acquire the mutex.
`fhxx` is the file handle and `chanxx`is the channel number.

\def INIT_CRIT_FILE(fhxx)
If `_IS_WINDOWS_` is defined this expands to code to initialise the file mutex
that protects the file head and file information from simultaneous
modification. `fhxx` is the file handle.

\def DEL_CRIT_FILE(fhxx)
if `_IS_WINDOWS_` is defined, this expands to code that deletes the file mutex
that protects the file header and file information. `fhxx` is the file handle.

\def START_CRIT_FILE(fhxx)
If `_IS_WINDOWS_` is defined, this expands to code that acquires the file mutex.
It is used to protect a region of code that uses and/or modifies the file
header or information. `fhxx` is the file handle.

\def END_CRIT_FILE(fhxx)
If `_IS_WINDOWS_` is defined, this expands to code that releases the file mutex.
`fhxx` is the file handle.
*/

#ifdef _IS_WINDOWS_
#define MTHREAD
#define INIT_CRIT_CHAN(fhxx, chanxx) InitializeCriticalSection(&g_SF[fhxx]->acSafe[chanxx])
#define DEL_CRIT_CHAN(fhxx, chanxx) DeleteCriticalSection(&g_SF[fhxx]->acSafe[chanxx])
#define START_CRIT_CHAN(fhxx, chanxx) EnterCriticalSection(&g_SF[fhxx]->acSafe[chanxx])
#define END_CRIT_CHAN(fhxx, chanxx) LeaveCriticalSection(&g_SF[fhxx]->acSafe[chanxx])
#define INIT_CRIT_FILE(fhxx) InitializeCriticalSection(&g_SF[fhxx]->fSafe)
#define DEL_CRIT_FILE(fhxx) DeleteCriticalSection(&g_SF[fhxx]->fSafe)
#define START_CRIT_FILE(fhxx) EnterCriticalSection(&g_SF[fhxx]->fSafe)
#define END_CRIT_FILE(fhxx) LeaveCriticalSection(&g_SF[fhxx]->fSafe)
#else
#undef MTHREAD
#define INIT_CRIT_CHAN(fhxx, chanxx)
#define DEL_CRIT_CHAN(fhxx, chanxx)
#define START_CRIT_CHAN(fhxx, chanxx)
#define END_CRIT_CHAN(fhxx, chanxx)
#define INIT_CRIT_FILE(fhxx)
#define DEL_CRIT_FILE(fhxx)
#define START_CRIT_FILE(fhxx)
#define END_CRIT_FILE(fhxx)
#endif

/*! Internal buffer for data writing */
typedef struct tagTWrBuf
{
    TDOF            lPos;               /*!< Position appointed on disk */
    int             nWrStart;           /*!< Start offset of wanted points */
    int             nWrPoints;          /*!< Number of wanted points */
    int             bChanged;           /*!< Flag for data points changed */
    int             bCommitted;         /*!< Flags that buffer is committed to disk */
    TpDataBlock     pBlk;               /*!< Pointer to allocated data block */
} TWrBuf;

/*! Stores read/write change */
typedef struct tagTChange               
{
    BOOLEAN         bKeep;              /*!< The change 'direction', TRUE for write */
    TSTime          lTime;              /*!< The time for the change */
} TChange;

/*! Structure used to speed up disk reading by finding the start posn fast */
typedef struct tagTLookup               
{
    TDOF            lPos;               /*!< The position in the file */
    TSTime          lStart;             /*!< The first time in this block */
    TSTime          lEnd;               /*!< The last time in this block */
} TLookup;

/*! Header for lookup tables */
typedef struct tagTSonLUTHead
{
    int             nSize;              /*!< The allocated size of the lookup table */
    int             nUsed;              /*!< Count of used items in lookup table */
    int             nInc;               /*!< Lookup table spacing (1, 2, 4, 8, 16, 32, 64...) */
    int             nGap;               /*!< index to the gap in the table or NOGAP if none */
    int             nCntAddEnd;         /*!< Count to when we add at end of table */
    int             nCntGapLow;         /*!< Count to when we add at low side of gap */
    int             nCntGapHigh;        /*!< count to when we add at high size of gap */
} TSonLUTHead;

/*! SON file lookup */
typedef struct tagTSonLUT
{
    TSonLUTHead     h;                  /*!< header for lut */
    TLookup*        pLooks;             /*!< Pointer to the lookup table itself */
} TSonLUT;

/*! Channel information */
typedef struct tagTChInfo
{
    TSonLUT         lut;                /*!< lookup table */
    TLookup         speedP;             /*!< Lookup info on last block read */

    TpDataBlock     bufferP;            /*!< pointer to single read buffer or NULL */
    WORD            bufferSz;           /*!< bufferP size in bytes */
    TDOF            lastPosnRead;       /*!< Last file position read to bufferP */   

    TSTime          lastWriteTime;      /*!< The last time flushed to disk */
    TSTime          firstBufferTime;    /*!< The earliest time in the write buffers */
    TDOF            lastWriteBlock;     /*!< Disk address of last written block */
    TDOF            lastSuccBlock;      /*!< Succ block value for last written block */
    int             nWritten;           /*!< Count of blocks actually written */
    int             bCurKeep;           /*!< Current write status for this channel */

    int             nIdeal;             /*!< The number of write buffers wanted for this channel */
    int             nBufs;              /*!< Number of write buffers we actually have */
    int             nSize;              /*!< The size of each write buffer */
    int             nFirst;             /*!< The first write buffer in use */
    TWrBuf*         pBufs;              /*!< The write buffers themselves */
    TSTime          lastValid;          /*!< Time for which data is complete */
    TChange         aChanges[CHANGES];  /*!< Array of stored write on/off changes */
} TChInfo;

typedef TChInfo FAR* TpChInfo;          /*!< Typedef for pointer to channel information*/

/*!
\brief Structure used to define a file for us to use.

If MTHREAD is defined we include space for SONABSMAXCHANS critical sections.
The critical sections are used for file-wide protection and channel protection.
We should NOT be creating SONABSMAXCHANS sections as this is wasteful. We should
be creating sufficient for the number of channels in the file. However, this is
the code that is currently in use. We should revise this to:
- Allocate space for desired number of channels (as chanP or pChInfo)
- Initialise and delete only these critical sections
*/
typedef struct TSonFile
{
    BOOLEAN     opened;       /*!< set true if the file is open */
    BOOLEAN     defined;      /*!< set true if headP and chanP are set */
    BOOLEAN     updateHead;   /*!< set TRUE to force update on close */
    BOOLEAN     bReadOnly;    /*!< if TRUE, no writes allowed */
    BOOLEAN     bNewFile;     /*!< Set TRUE if a new file */
    TFH         fileHnd;      /*!< file identifier - type depends on the OS */
#ifdef SON64DLL
    TpS64       pS64;         /*!< points at and owns any 64-bit son file */
#endif
    TpFileHead  headP;        /*!< pointer to area for file head */
    TpChannel   chanP;        /*!< pointer to area for channels */
    TpChInfo    pChInfo;      /*!< Per channel extra information */
    TDOF        lFileSize;    /*!< Effective file size during writing */
    S32I        lDiskVer;     /*!< SON version in the file header on disk */
#ifdef MTHREAD
    CRITICAL_SECTION fSafe;   /*!< Mutex protecting the file pointer and lFileSize info */
    CRITICAL_SECTION acSafe[SONABSMAXCHANS];   /*!< Per-channel mutex object */
#endif
} TSonFile;
#endif

