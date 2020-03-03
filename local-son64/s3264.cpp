// s3264.cpp
/*
    Copyright (C) Cambridge Electronic Design Limited 2012-2019
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

//! \file s3264.cpp
//! \brief Code to make a son64 file look like a read-only son32 file
/*!
This file mimics all the SONxxxx 32-bit functions with S32xxxx functions that map onto
a 64-bit file. We only intend to make reading work. If writing were to work too, then
that is a bonus.

This file is designed to be compatible with the son32 dll, so we want to be able to include
it with that, and that does not use namespaces, so we will avoid them here, too. The idea is
that if the son64.dll file is in the same folder as the son32.dll, the son32.dll file will
locate son64.dll on startup, then forward all calls that pertain to a 64-bit file through
son64.dll, allowing read-only access to the first TSTIME_MAX ticks of the 64-bit file.
*/

// Stop the Microsoft compiler moaning about strncpy (which is used as safely as possible).
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "machine.h"
#include "s64priv.h"
#include "s64filt.h"
#include "s64iter.h"
#include "s3264.h"

#include <string>
#include <vector>
#include <iostream>

using namespace ceds64;
using namespace std;

/*! \brief Map a 64-bit SON library return code to a 32-bit code.
\internal
\param s64Err The \ref Son64 error code to map to a SON library code.
             -1 maps to -1, so returns from PrevNTime() work.
             Passed as an int in so that when a TSTime is passed in it does not get truncated.
\return      The equivalent SON error code. Unknown codes and positive values are
             returned unchanged.
*/
static int S32Err(int s64Err)
{
    switch (s64Err)
    {
    case S64_OK: return 0;

    case CALL_AGAIN:    // should never be returned
    case NO_FILE:       return SON_NO_FILE;
    case NO_BLOCK:
    case PAST_EOF:      return SON_PAST_EOF;
    case NO_ACCESS:     return SON_NO_ACCESS;
    case NO_CHANNEL:    return SON_NO_CHANNEL;
    case CHANNEL_USED:  return SON_CHANNEL_USED;
    case CHANNEL_TYPE:  return SON_NO_CHANNEL;
    case WRONG_FILE:    return SON_WRONG_FILE;
    case NO_EXTRA:      return SON_NO_EXTRA;
    case CORRUPT_FILE:  return SON_CORRUPT_FILE;
    case PAST_SOF:      return SON_PAST_SOF;
    case READ_ONLY:     return SON_READ_ONLY;
    case BAD_PARAM:     return SON_BAD_PARAM;
    case NO_MEMORY:     return SON_OUT_OF_MEMORY;
    case BAD_READ:      return SON_BAD_READ;
    case BAD_WRITE:     return SON_BAD_WRITE;
    default:            return s64Err;      // so +ve stays +ve
    }
}

//! Convert an opaque sonfile pointer to a CSon64File pointer
/*!
\param p    An opaque pointer generated by an open routine
\return     A CSon64File pointer (actually a TSon64File pointer)
*/
inline CSon64File* SF(TpS64 p){return static_cast<CSon64File*>(p);}

//! Open an existing file for reading (replacement for SONOpenOldFile())
/*!
\param name The name of the file
\param iOpenMode The file open mode
\return An opaque pointer to the file or nullptr if we fail
*/
SONAPI(TpS64) S32OpenOldFile(TpCFName name, int iOpenMode)
{
    TSon64File *p = new TSon64File;
    if (p)
    {
        int iErr = p->Open(name, iOpenMode > 1 ? -1 : iOpenMode);
        if (iErr)
        {
            delete p;
            p = nullptr;
        }
    }
    return static_cast<TpS64>(p);
}

//! This is a READ ONLY interface, so no creating of files; this always fails.
SONAPI(TpS64) S32CreateFileEx(TpCFName name, int nChannels, WORD extra, int iBigFile)
{
    return nullptr;
}

//! This is a READ ONLY interface, so no creating of files; this always fails.
SONAPI(TpS64) S32CreateFile(TpCFName name, int nChannels, WORD extra)
{
    return nullptr;
}

//! This is a READ ONLY interface, so no creating of files; this always fails.
SONAPI(TpS64) S32OpenNewFile(TpCFName name, short fMode, WORD extra)
{
    return nullptr;
}

//! This is a READ ONLY interface, so always read only.
SONAPI(BOOLEAN) S32CanWrite(TpS64 fh)
{
    return false;
}

//! Close the file
SONAPI(short) S32CloseFile(TpS64 fh)
{
    if (fh)
    {
        short sErr = S32Err(SF(fh)->Close());
        delete SF(fh);
        return sErr;
    }
    else
        return SON_NO_FILE;
}

//! This is a READ ONLY interface, so no changing files; this always fails.
SONAPI(short) S32EmptyFile(TpS64 fh)
{
    return SON_READ_ONLY;
}

//! This is a READ ONLY interface, so no setting write buffer sizes.
SONAPI(short) S32SetBuffSpace(TpS64 fh)
{
    return SON_READ_ONLY;
}

//! \brief Get the next free channel, but this is read only, so little point.
SONAPI(short) S32GetFreeChan(TpS64 fh)
{
    if (fh)
        return S32Err(SF(fh)->GetFreeChan());
    else
        return SON_NO_FILE;
}

//! This is not allowed in a read only file
SONAPI(void) S32SetFileClock(TpS64 fh, WORD usPerTime, WORD timePerADC)
{
}

//! This is not allowed in a read only file
SONAPI(short) S32SetADCChan(TpS64 fh, WORD chan, short sPhyCh, short dvd,
                 int lBufSz, TpCStr szCom, TpCStr szTitle, float fRate,
                 float scl, float offs, TpCStr szUnt)
{
    return SON_READ_ONLY;
}

//! This is not allowed in a read only file
SONAPI(short) S32SetADCMarkChan(TpS64 fh, WORD chan, short sPhyCh, short dvd,
                 int lBufSz, TpCStr szCom, TpCStr szTitle, float fRate, float scl,
                 float offs, TpCStr szUnt, WORD points, short preTrig)
{
    return SON_READ_ONLY;
}

//! This is not allowed in a read only file
SONAPI(short) S32SetWaveChan(TpS64 fh, WORD chan, short sPhyCh, TSTime dvd,
                 int lBufSz, TpCStr szCom, TpCStr szTitle,
                 float scl, float offs, TpCStr szUnt)
{
    return SON_READ_ONLY;
}

//! This is not allowed in a read only file
SONAPI(short) S32SetWaveMarkChan(TpS64 fh, WORD chan, short sPhyCh, TSTime dvd,
                 int lBufSz, TpCStr szCom, TpCStr szTitle, float fRate, float scl,
                 float offs, TpCStr szUnt, WORD points, short preTrig, int nTrace)
{
    return SON_READ_ONLY;
}

//! This is not allowed in a read only file
SONAPI(short) S32SetRealMarkChan(TpS64 fh, WORD chan, short sPhyCh,
                 int lBufSz, TpCStr szCom, TpCStr szTitle, float fRate,
                 float min, float max, TpCStr szUnt, WORD points)
{
    return SON_READ_ONLY;
}

//! This is not allowed in a read only file
SONAPI(short) S32SetTextMarkChan(TpS64 fh, WORD chan, short sPhyCh,
                 int lBufSz, TpCStr szCom, TpCStr szTitle,
                 float fRate, TpCStr szUnt, WORD points)
{
    return SON_READ_ONLY;
}

//! This has no effect in a read only file.
SONAPI(void) S32SetInitLow(TpS64 fh, WORD chan, BOOLEAN bLow)
{
}

//! This is not allowed in a read only file
SONAPI(short) S32SetEventChan(TpS64 fh, WORD chan, short sPhyCh,
                 int lBufSz, TpCStr szCom, TpCStr szTitle, float fRate, ::TDataKind evtKind)
{
    return SON_READ_ONLY;
}

//! This is not allowed in a read only file
SONAPI(short) S32SetBuffering(TpS64 fh, int nChan, int nBytes)
{
    return SON_READ_ONLY;
}

//! This is not allowed in a read only file
SONAPI(short) S32UpdateStart(TpS64 fh)
{
    return SON_READ_ONLY;
}

//! This has no effect in a read only file
SONAPI(void) S32SetFileComment(TpS64 fh, WORD which, TpCStr szFCom)
{
}

// Convert a string to a C style string with a maximum size
static void str2sz(TpStr sz, short sMax, const string& s)
{
    if (sMax > 0)
    {
        strncpy(sz, s.c_str(), sMax);
        sz[sMax-1] = 0;
    }
}

//! Get the file comment
SONAPI(void) S32GetFileComment(TpS64 fh, WORD which, TpStr pcFCom, short sMax)
{
    if (fh)
        SF(fh)->GetFileComment((int)which, sMax, pcFCom);
}

//! Not allowed in a read only file
SONAPI(void) S32SetChanComment(TpS64 fh, WORD chan, TpCStr szCom)
{
}

//! Read the channel comment
SONAPI(void) S32GetChanComment(TpS64 fh, WORD chan, TpStr pcCom, short sMax)
{
    if (fh)
        SF(fh)->GetChanComment(chan, sMax, pcCom);
}

//! This is not allowed in a read only file
SONAPI(void) S32SetChanTitle(TpS64 fh, WORD chan, TpCStr szTitle)
{
}

//! Read the channel title
SONAPI(void) S32GetChanTitle(TpS64 fh, WORD chan, TpStr pcTitle)
{
    if (fh)
        SF(fh)->GetChanTitle(chan, SON_TITLESZ + 1, pcTitle);
}

//! Collect the rate and range.
SONAPI(void) S32GetIdealLimits(TpS64 fh, WORD chan, TpFloat pfRate, TpFloat pfMin, TpFloat pfMax)
{
    if (fh)
    {
        *pfRate = static_cast<float>(SF(fh)->IdealRate(chan));
        S32YRange(fh, chan, pfMin, pfMax);
    }
}

//! Always 1 as time base determines the resolution.
SONAPI(WORD) S32GetusPerTime(TpS64 fh)
{
    return 1;
}

//! Always 1 as does not exist in Son64.
SONAPI(WORD) S32GetTimePerADC(TpS64 fh)
{
    return 1;
}

//! No effect as READ ONLY
SONAPI(void) S32SetADCUnits(TpS64 fh, WORD chan, TpCStr szUnt)
{
}

//! No effect as READ ONLY
SONAPI(void) S32SetADCOffset(TpS64 fh, WORD chan, float offset)
{
}

//! No effect as READ ONLY
SONAPI(void) S32SetADCScale(TpS64 fh, WORD chan, float scale)
{
}

//! Read waveform channel information.
SONAPI(void) S32GetADCInfo(TpS64 fh, WORD chan, TpFloat scale, TpFloat offset,
                 TpStr pcUnt, TpWORD points, TpShort preTrig)
{
    if (fh)
    {
        if (scale)
        {
            double dScale = 1.0;
            SF(fh)->GetChanScale(chan, dScale);
            *scale = (float)dScale;
        }
        
        if (offset)
        {
            double dOffset = 0.0;
            SF(fh)->GetChanOffset(chan, dOffset);
            *offset = (float)dOffset;
        }

        S32GetExtMarkInfo(fh, chan, pcUnt, points, preTrig);
    }
}

//! Read extended marker information.
SONAPI(void) S32GetExtMarkInfo(TpS64 fh, WORD chan, TpStr pcUnt,
                 TpWORD points, TpShort preTrig)
{
    if (fh)
    {
        if (pcUnt)
            SF(fh)->GetChanUnits(chan, SON_UNITSZ + 1, pcUnt);

        if (points || preTrig)
        {
            size_t rows, cols;
            int nPre;
            nPre = SF(fh)->GetExtMarkInfo(chan, &rows, &cols);
            if (nPre >= 0)
            {
                if (points)
                    *points = static_cast<short>(rows);
                if (preTrig)
                    *preTrig = static_cast<short>(nPre);
            }
        }
    }
}

//! This is a READ ONLY interface, so no changing files; this always fails.
SONAPI(short) S32WriteEventBlock(TpS64 fh, WORD chan, TpSTime plBuf, int count)
{
    return SON_READ_ONLY;
}

//! This is a READ ONLY interface, so no changing files; this always fails.
SONAPI(short) S32WriteMarkBlock(TpS64 fh, WORD chan, TpMarker pM, int count)
{
    return SON_READ_ONLY;
}

//! This is a READ ONLY interface, so no changing files; this always fails.
SONAPI(TSTime) S32WriteADCBlock(TpS64 fh, WORD chan, TpAdc psBuf, int count, TSTime sTime)
{
    return SON_READ_ONLY;
}

//! This is a READ ONLY interface, so no changing files; this always fails.
SONAPI(short) S32WriteExtMarkBlock(TpS64 fh, WORD chan, TpMarker pM, int count)
{
    return SON_READ_ONLY;
}

//! This is a READ ONLY interface, so no changing files; this always fails.
SONAPI(short) S32Save(TpS64 fh, int nChan, TSTime sTime, BOOLEAN bKeep)
{
    return SON_READ_ONLY;
}

//! This is a READ ONLY interface, so no changing files; this always fails.
SONAPI(short) S32SaveRange(TpS64 fh, int nChan, TSTime sTime, TSTime eTime)
{
    return SON_READ_ONLY;
}

//! This is a READ ONLY interface, so no changing files; this always fails.
SONAPI(short) S32KeepRange(TpS64 fh, int nChan, TSTime sTime, TSTime eTime, BOOLEAN bKeep)
{
    return SON_READ_ONLY;
}

//! This is a READ ONLY interface, so no changing files; this always fails.
SONAPI(short) S32KillRange(TpS64 fh, int nChan, TSTime sTime, TSTime eTime)
{
    return SON_READ_ONLY;
}

//! This is a READ ONLY interface, so no changing files; this always fails.
SONAPI(short) S32NoSaveList(TpS64 fh, WORD wChan, TSTime* pTimes)
{
    return SON_READ_ONLY;
}

//! This is a READ ONLY interface, so no changing files; this always fails.
SONAPI(short) S32IsSaving(TpS64 fh, int nChan)
{
    return false;
}

//! This is usually used only when sampling, so we ignore it.
SONAPI(DWORD) S32FileBytes(TpS64 fh)
{
    return 0;
}

//! This is usually used only when sampling, so we ignore it.
SONAPI(DWORD) S32ChanBytes(TpS64 fh, WORD chan)
{
    return 0;
}

//! This is a READ ONLY interface, so no changing files; this always fails.
SONAPI(short) S32LatestTime(TpS64 fh, WORD chan, TSTime sTime)
{
    return SON_READ_ONLY;
}

//! This is a READ ONLY interface, so no changing files; this always fails.
SONAPI(short) S32CommitIdle(TpS64 fh)
{
    return SON_READ_ONLY;
}

//! This is a READ ONLY interface, so no changing files; this always fails.
SONAPI(short) S32CommitFile(TpS64 fh, BOOLEAN bDelete)
{
    return SON_READ_ONLY;
}

//! This is a READ ONLY interface, so no changing files; this always fails.
SONAPI(short) S32CommitFileEx(TpS64 fh, int flags)
{
    return SON_READ_ONLY;
}


/*! \brief Create a matching filter mask and return a pointer to it.

This code assumes that the memory layout of the masks is the same. The new
filters use 32-bit ints at the bottom to speed things up. The old uses bytes.
These will layout the same in a little-endian system (which is all we use) but
if you ever use big-endian there may be a problem.
\param pFM32  Input filter mask. If not set, then we return nullptr
\param f      Where we create the equivalent mask.
\return       nullptr if an error, else points at \a f, the filter passed in.
*/
static const CSFilter* S64FM(const TpFilterMask pFM32, CSFilter &f)
{
    if (!pFM32)
        return nullptr;

    for (int i=0; i<3; ++i)     // copy the layers of the mask we can use
        f.SetElements(&(pFM32->aMask[i]), i);
    f.SetMode((pFM32->lFlags & SON_FMASK_ORMODE) ? CSFilter::eM_or : CSFilter::eM_and);
    return &f;
}

//! Read event data as TSTime.
SONAPI(int) S32GetEventData(TpS64 fh, WORD chan, TpSTime plTimes, int max,
                  TSTime sTime, TSTime eTime, TpBOOL levLowP, 
                  TpFilterMask pFiltMask)
{
    if (!fh)
        return SON_NO_FILE;

    vector<TSTime64> buff(max);     // space for data
    TSTime64 tUpto = (TSTime64)eTime + 1;
    int n;
    if ((SF(fh)->ChanKind(chan) == ceds64::EventBoth) && levLowP)
    {
        bool bLevel;
        n = SF(fh)->ReadLevels(chan, &buff[0], max, sTime, tUpto, bLevel);
        *levLowP = bLevel;
    }
    else
    {
        CSFilter f;
        const CSFilter* pf = S64FM(pFiltMask, f);
        n = SF(fh)->ReadEvents(chan, &buff[0], max, sTime, tUpto, pf);
    }

    if (n > 0)
    {
        for (int i=0; i< n; ++i)
            plTimes[i] = static_cast<TSTime>(buff[i]);
    }
    return S32Err(n);
}

// Horrible hack to copy a marker with reasonable efficiency
static void CopyMark(::TMarker& m32, const ceds64::TMarker& m64)
{
    m32.mark = static_cast<TSTime>(m64.m_time);
    *reinterpret_cast<int32_t*>(m32.mvals) = m64.m_int[0];
}

static void CopyExtMark(::TMarker& m32, const ceds64::TExtMark& m64, size_t nExtra)
{
    CopyMark(m32, m64);                 // copy the time
    memcpy(&m32+1, &m64+1, nExtra);     // copy attached data
}

//! Read marker data.
SONAPI(int) S32GetMarkData(TpS64 fh, WORD chan,TpMarker pMark, int max,
                  TSTime sTime,TSTime eTime, TpFilterMask pFiltMask)
{
    if (!fh)
        return SON_NO_FILE;

    vector<ceds64::TMarker> buff(max);     // space for data
    TSTime64 tUpto = (TSTime64)eTime + 1;
    CSFilter f;
    const CSFilter* pf = S64FM(pFiltMask, f);
    int n = SF(fh)->ReadMarkers(chan, &buff[0], max, sTime, tUpto, pf);

    if (n > 0)
    {
        for (int i=0; i< n; ++i)
            CopyMark(pMark[i], buff[i]);
    }
    return S32Err(n);
}

//! Read extended marker data.
SONAPI(int) S32GetExtMarkData(TpS64 fh, WORD chan, TpMarker pMark, int max,
                  TSTime sTime,TSTime eTime, TpFilterMask pFiltMask)
{
    if (!fh)
        return SON_NO_FILE;

    size_t rows, cols;
    int nPre = SF(fh)->GetExtMarkInfo(chan, &rows, &cols);
    if (nPre < 0)
        return S32Err(nPre);

    // Get the stride in bytes for input (SON64) and output (SON32)
    size_t iSz = SF(fh)->ItemSize(chan);
    size_t oSz = iSz - (sizeof(ceds64::TMarker) - sizeof(::TMarker));
    vector<char> buff(max*iSz);     // space for data
    TExtMark* pEM = reinterpret_cast<TExtMark*>(&buff[0]);
    TSTime64 tUpto = (TSTime64)eTime + 1;
    CSFilter f;
    const CSFilter* pf = S64FM(pFiltMask, f);
    int n = SF(fh)->ReadExtMarks(chan, pEM, max, sTime, tUpto, pf);
    if (n > 0)
    {
        db_iterator<TExtMark> iMark(pEM, iSz);
        db_iterator<::TMarker, true> oMark(pMark, oSz);
        if ((cols<2) || (SF(fh)->ChanKind(chan) == ceds64::AdcMark))
        {
            for (int i=0; i< n; ++i)
                CopyExtMark(oMark[i], iMark[i], iSz-sizeof(ceds64::TMarker));
        }
        else
            return SON_NO_CHANNEL;  // cannot cope with this channel type
    }
    return S32Err(n);
}

//! Read contiguous waveform data as shorts.
/*!
\param fh       The file handle to identify the file.
\param chan     The channel number in the file of a channel that can be read as a waveform.
\param adcDataP Points at the buffer to receive the data.
\param max      The maximum number of contiguous values to return.
\param sTime    The start time of the read.
\param eTime    The last acceptable time of the read.
\param pbTime   If not nullptr, the time of the first read item is returned here. If there
                are no returned values, this is unchanged.
\param nTr      The 0-based trace number to use when reading from multi-trace WaveMark data.
\param pFiltMask Marker mask (or nullptr) used to filter WaveMark data.
\return         The number of data points read, or a negative error code.
*/
SONAPI(int) S32GetADCData(TpS64 fh,WORD chan,TpAdc adcDataP, int max,
                  TSTime sTime,TSTime eTime,TpSTime pbTime, 
                  int nTr, TpFilterMask pFiltMask)
{
    if (!fh)
        return SON_NO_FILE;
    TSTime64 tUpto = (TSTime64)eTime + 1;
    CSFilter f;
    const CSFilter* pf = S64FM(pFiltMask, f);
    f.SetColumn(nTr);                     // The trace number is kept in the filter
    TSTime64 tFirst;
    int n = SF(fh)->ReadWave(chan, adcDataP, max, sTime, tUpto, tFirst, pf);
    if ((n>0) && pbTime)
        *pbTime = static_cast<TSTime>(tFirst);
    return S32Err(n);
}

//! Get the size of the extra data region.
SONAPI(int) S32GetExtraDataSize(TpS64 fh)
{
    if (!fh)
        return SON_NO_FILE;
    return (int)SF(fh)->GetExtraDataSize();
}

//! Get the file version.
SONAPI(int) S32GetVersion(TpS64 fh)
{
    if (!fh)
        return SON_NO_FILE;
    return SF(fh)->GetVersion();
}

//! Read the extra data in the file header. No write as READ ONLY.
SONAPI(short) S32GetExtraData(TpS64 fh, TpVoid buff, WORD bytes,
                  WORD offset, BOOLEAN writeIt)
{
    if (!fh)
        return SON_NO_FILE;
    if (writeIt)
        return SON_READ_ONLY;

    return SF(fh)->GetExtraData(buff, bytes, offset);
}

//! This is a READ ONLY interface, so no changing files; this always fails.
SONAPI(short) S32SetMarker(TpS64 fh, WORD chan, TSTime time, TpMarker pMark,
                  WORD size)
{
    return SON_READ_ONLY;
}

//! I do not know why we let the user delete a channel! Surely READ ONLY access.
SONAPI(short)  S32ChanDelete(TpS64 fh, WORD chan)
{
    if (!fh)
        return SON_NO_FILE;
    return S32Err(SF(fh)->ChanDelete(chan));

}

//! Get the channel type.
SONAPI(::TDataKind) S32ChanKind(TpS64 fh, WORD chan)
{
    if (!fh)
        return ::ChanOff;
    return static_cast<::TDataKind>(SF(fh)->ChanKind(chan));
}

//! Get the channel divider
SONAPI(TSTime) S32ChanDivide(TpS64 fh, WORD chan)
{
    if (!fh)
        return SON_NO_FILE;
    TSTime64 tDvd = SF(fh)->ChanDivide(chan);
    if (tDvd<0)
        return S32Err(static_cast<int>(tDvd));
    return (tDvd > TSTIME_MAX) ? TSTIME_MAX : static_cast<TSTime>(tDvd);
}

//! Mangle the size differences
SONAPI(WORD) S32ItemSize(TpS64 fh, WORD chan)
{
    if (!fh)
        return SON_NO_FILE;
    WORD itemSz = static_cast<WORD>(SF(fh)->ItemSize(chan));
    switch (S32ChanKind(fh, chan))
    {
    case ::Marker:
    case ::AdcMark:
    case ::RealMark:
    case ::TextMark:
        itemSz -= sizeof(ceds64::TMarker)-sizeof(::TMarker);
    }
    return itemSz;
}

//! Get the maximum time in the channel.
SONAPI(TSTime) S32ChanMaxTime(TpS64 fh, WORD chan)
{
    if (!fh)
        return SON_NO_FILE;
    TSTime64 tMax = SF(fh)->ChanMaxTime(chan);
    if (tMax<0)
        return S32Err(static_cast<int>(tMax));
    return (tMax > TSTIME_MAX) ? TSTIME_MAX : static_cast<TSTime>(tMax);
}

//! Get the maximum time in the file.
SONAPI(TSTime) S32MaxTime(TpS64 fh)
{
    if (!fh)
        return SON_NO_FILE;
    TSTime64 tMax = SF(fh)->MaxTime();
    if (tMax<0)
        return S32Err(static_cast<int>(tMax));
    return (tMax > TSTIME_MAX) ? TSTIME_MAX : static_cast<TSTime>(tMax);
}

//! Get the time of the previous item
SONAPI(TSTime) S32LastTime(TpS64 fh, WORD wChan, TSTime sTime, TSTime eTime,
                    TpVoid pvVal, TpMarkBytes pMB,
                    TpBOOL pbMk, TpFilterMask pFiltMask)
{
    if (!fh)
        return SON_NO_FILE;
    CSFilter f;
    const CSFilter* pf = S64FM(pFiltMask, f);

    TSTime64 t = SF(fh)->PrevNTime(wChan, sTime, eTime, 1, pf);
    if (t < 0)
        return S32Err(static_cast<int>(t));

    ceds64::TDataKind kind = SF(fh)->ChanKind(wChan);
    switch (kind)
    {
    case ceds64::Adc:
        {
            if (pvVal)
            {
                short s;
                TSTime64 tFirst;
                SF(fh)->ReadWave(wChan, &s, 1, t, t+1, tFirst);
                *(short*)pvVal = s;
            }
        }
        break;
    case ceds64::RealWave:
        {
            if (pvVal)
            {
                float f;
                TSTime64 tFirst;
                SF(fh)->ReadWave(wChan, &f, 1, t, t+1, tFirst);
                *(float*)pvVal = f;
            }
        }
        break;
    case ceds64::EventBoth:
        {
            if (pvVal)
            {
                ceds64::TMarker mark;
                SF(fh)->ReadMarkers(wChan, &mark, 1, t, t+1, pf);
                *(short*)pvVal = mark.m_code[0];
            }
        }
        break;
    case ceds64::Marker:
    case ceds64::RealMark:
    case ceds64::TextMark:
    case ceds64::AdcMark:
        {
            if (pbMk)
                *pbMk = true;
            if (pMB)
            {
                ceds64::TMarker mark;
                if (SF(fh)->ReadMarkers(wChan, &mark, 1, t, t+1, pf) == 1)
                {
                    (*pMB)[0] = mark.m_code[0];
                    (*pMB)[1] = mark.m_code[1];
                    (*pMB)[2] = mark.m_code[2];
                    (*pMB)[3] = mark.m_code[3];
                }
            }
        }
        break;
    }
    return static_cast<TSTime>(t);          // t cannot exceed TSTIME_MAX
}

//! Get the time of the item lPoints back from a time.
SONAPI(TSTime) S32LastPointsTime(TpS64 fh, WORD wChan, TSTime sTime, TSTime eTime,
                    int lPoints, BOOLEAN bAdc, TpFilterMask pFiltMask)
{
    if (!fh)
        return SON_NO_FILE;
    CSFilter f;
    const CSFilter* pf = S64FM(pFiltMask, f);

    TSTime64 t = SF(fh)->PrevNTime(wChan, sTime, eTime, lPoints, pf, bAdc != 0);
    if (t < 0)
        return S32Err(static_cast<int>(t));
    return static_cast<TSTime>(t);          // t cannot exceed TSTIME_MAX
}

//! Get the number of channels that can be accomodated in the file.
SONAPI(int) S32MaxChans(TpS64 fh)
{
    if (!fh)
        return SON_NO_FILE;
    return S32Err(SF(fh)->MaxChans());
}

//! Get the physical port associated with the channel.
SONAPI(int) S32PhyChan(TpS64 fh, WORD wChan)
{
    if (!fh)
        return SON_NO_FILE;
    return S32Err(SF(fh)->PhyChan(wChan));
}

//! Get the ideal rate for the channel.
SONAPI(float) S32IdealRate(TpS64 fh, WORD wChan, float fIR)
{
    if (!fh)
        return SON_NO_FILE;
    return (float)SF(fh)->IdealRate(wChan);
}

//! Get the suggested Y range for the channel.
SONAPI(void) S32YRange(TpS64 fh, WORD chan, TpFloat pfMin, TpFloat pfMax)
{
    if (!fh)
        return;
    
    double dLo, dHi;
    SF(fh)->GetChanYRange(chan, dLo, dHi);
    if (pfMin)
        *pfMin = (float)dLo;
    if (pfMax)
        *pfMax = (float)dHi;

}

//! This is a READ ONLY interface, so no changing files; this always fails.
SONAPI(int) S32YRangeSet(TpS64 fh, WORD chan, float fMin, float fMax)
{
    return SON_READ_ONLY;
}

//! Report the maximum items in a buffer.
SONAPI(int) S32MaxItems(TpS64 fh, WORD chan)
{
    if (!fh)
        return 0;
    int itemSz = (int)SF(fh)->ItemSize(chan);   // itemSz is 0 or  < DBSize
    return itemSz ? DBSize / itemSz : 0;

}

//! Report the physical buffer size; either 0 if no channel or DBSize.
SONAPI(int) S32PhySz(TpS64 fh, WORD chan)
{
    return S32ChanKind(fh, chan) != ::ChanOff ? DBSize : 0;
}

//! We do not attempt to emulate the blocks of Son32 files. Just say 0 or 1.
SONAPI(int) S32Blocks(TpS64 fh, WORD chan)
{
    return S32ChanKind(fh, chan) != ::ChanOff ? 1 : 0;
}

//! This concept does not exist.
SONAPI(int) S32DelBlocks(TpS64 fh, WORD chan)
{
    return 0;
}

//! This is a READ ONLY interface, so no changing files; this always fails.
SONAPI(int) S32SetRealChan(TpS64 fh, WORD chan, short sPhyChan, TSTime dvd,
                 int lBufSz, TpCStr szCom, TpCStr szTitle,
                 float scale, float offset, TpCStr szUnt)
{
    return SON_READ_ONLY;
}

//! This is a READ ONLY interface, so no changing files; this always fails.
SONAPI(TSTime) S32WriteRealBlock(TpS64 fh, WORD chan, TpFloat pfBuff, int count, TSTime sTime)
{
    return SON_READ_ONLY;
}

//! Read contiguous waveform data as floats.
/*!
\param fh       The file handle to identify the file.
\param chan     The channel number in the file of a channel that can be read as a waveform.
\param pfData   Points at the buffer to receive the data.
\param max      The maximum number of contiguous values to return.
\param sTime    The start time of the read.
\param eTime    The last acceptable time of the read.
\param pbTime   If not nullptr, the time of the first read item is returned here. If there
                are no returned values, this is unchanged.
\param nTr      The 0-based trace number to use when reading from multi-trace WaveMark data.
\param pFiltMask Marker mask (or nullptr) used to filter WaveMark data.
\return         The number of data points read, or a negative error code.
*/
SONAPI(int) S32GetRealData(TpS64 fh, WORD chan, TpFloat pfData, int max,
                  TSTime sTime,TSTime eTime,TpSTime pbTime, 
                  int nTr, TpFilterMask pFiltMask)
{
    if (!fh)
        return SON_NO_FILE;
    TSTime64 tUpto = (TSTime64)eTime + 1;
    CSFilter f;
    const CSFilter* pf = S64FM(pFiltMask, f);
    f.SetColumn(nTr);                     // The trace number is kept in the filter
    TSTime64 tFirst;
    int n = SF(fh)->ReadWave(chan, pfData, max, sTime, tUpto, tFirst, pf);
    if ((n>0) && pbTime)
        *pbTime = static_cast<TSTime>(tFirst);
    return S32Err(n);
}

/*! \brief Get the file time and date (no set as READ ONLY).

The ceds64 and ceds32 TTimeDate TSONTimeDate structures are bitwise identical
Return 1 if OK, 0 if null, -ve for bad
*/
SONAPI(int) S32TimeDate(TpS64 fh, TSONTimeDate* pTDGet, const TSONTimeDate* pTDSet)
{
    if (!fh)
        return SON_NO_FILE;
    if (pTDSet && !pTDGet)
        return SON_READ_ONLY;
    static_assert(sizeof(TTimeDate) == sizeof(TSONTimeDate), "TTimeDate size mismatch");
    return S32Err(SF(fh)->TimeDate(reinterpret_cast<TTimeDate*>(pTDGet), nullptr));
}

//! Get the file timebase
SONAPI(double) S32TimeBase(TpS64 fh, double dTB)
{
    if (!fh)
        return SON_NO_FILE;
    if (dTB > 0.0)
        return SON_READ_ONLY;
    return SF(fh)->GetTimeBase();
}

/*! \brief Get or set the file creator.

We know that ceds64::TCreator is bit for bit equivalent to ceds32::TSONCreator
*/
SONAPI(int) S32AppID(TpS64 fh, TSONCreator* pCGet, const TSONCreator* pCSet)
{
    if (!fh)
        return SON_NO_FILE;
    if (pCSet && !pCGet)
        return SON_READ_ONLY;

    static_assert(sizeof(TCreator) == sizeof(TSONCreator), "TCreator size mismatch");
    return S32Err(SF(fh)->AppID(reinterpret_cast<TCreator*>(pCGet), nullptr));
}

//! Get the channel interleave (traces) for extended marker data.
SONAPI(int) S32ChanInterleave(TpS64 fh, WORD chan)
{
    if (!fh)
        return SON_NO_FILE;
    size_t rows, cols;
    int nPre = SF(fh)->GetExtMarkInfo(chan, &rows, &cols);
    if (nPre < 0)
        return S32Err(nPre);
    return (SF(fh)->ChanKind(chan) == ceds64::AdcMark) ? static_cast<int>(cols) : 1;
}

//! Get the alignment setting for extended marker data (cannot set as READ ONLY)
SONAPI(int) S32ExtMarkAlign(TpS64 fh, int n)
{
    if (!fh)
        return SON_NO_FILE;
    switch (n)
    {
    case -2: return 1;  // say aligned (as we are!)
    case -1: return 1;  // say aligned flag is set as always is aligned
    default: return SON_READ_ONLY; // cannot change state
    }
}

//! Get the file size as an integer (may be trucated)
SONAPI(int) S32FileSize(TpS64 fh)
{
    if (!fh)
        return 0;
    uint64_t n = SF(fh)->FileSize();
    return n > LONG_MAX ? LONG_MAX : static_cast<int>(n);
}

//! Get the file size as a double.
SONAPI(double) S32FileSizeD(TpS64 fh)
{
    if (!fh)
        return 0;
    uint64_t n = SF(fh)->FileSize();
    return static_cast<double>(n);

}

//! Get the channel bytes as a double.
SONAPI(double) S32ChanBytesD(TpS64 fh, WORD chan)
{
    if (!fh)
        return 0;
    uint64_t n = SF(fh)->ChanBytes(chan);
    return static_cast<double>(n);
}

//!  Report if this is a big file
SONAPI(int) S32IsBigFile(TpS64 fh)
{
    return fh ? 2 : SON_NO_FILE;    // 1 is big file, 2 is 64-bit huge file!
}

//! This is a READ ONLY interface, so no changing files; this always fails.
SONAPI(int) S32UpdateHeader(TpS64 fh)
{
    return SON_READ_ONLY;
}

//! This is a READ ONLY interface, so no changing files; this always fails.
SONAPI(int) S32UpdateMaxTimes(TpS64 fh)
{
    return SON_READ_ONLY;
}

//! This is a READ ONLY interface, so no changing files; this always fails.
SONAPI(void) S32ExtendMaxTime(TpS64 fh, TSTime time)
{
}

//! This is a READ ONLY interface, so no changing files; this always fails.
SONAPI(void) S32SetPhySz(TpS64 fh, WORD wChan, int lSize)
{
}

#ifndef S64_NOTDLL
/****************************************************************************
   FUNCTION: DllMain(HANDLE, DWORD, LPVOID)

   PURPOSE:  DllMain is called by Windows when
             the DLL is initialized, Thread Attached, and other times.
             Refer to SDK documentation, as to the different ways this
             may be called.
             
             The DllMain function should perform additional initialization
             tasks required by the DLL.  In this example, no initialization
             tasks are required.  DllMain should return a value of 1 if
             the initialization is successful.
           
*******************************************************************************/
 static int iDoneInit = 0;
//! The Main entry point of the DLL.
INT APIENTRY DllMain(HANDLE hInst, DWORD ul_reason_being_called, LPVOID lpReserved) {
    if ((ul_reason_being_called == DLL_PROCESS_ATTACH) && (iDoneInit==0))
    {
        SONInitFiles();
        iDoneInit = 1;
    }

    if (ul_reason_being_called == DLL_PROCESS_DETACH)
        SONCleanUp();        // Try to undo memory allocation as we are quitting

    return 1;

	UNREFERENCED_PARAMETER(hInst);
	UNREFERENCED_PARAMETER(lpReserved);
}

//! Windows Exit Procedure
void WINAPI WEP(int nParameter)
{
	UNREFERENCED_PARAMETER(nParameter);

    return;
}
#endif

#if (defined(LINUX) && defined(GNUC))
// Added this to the original CED sources so shared .so libs init and exit correctly.
// These are not executed in static libs, the app has to call them directly.
// --dale
static int iDoneInit = 0;
void before_main() __attribute__ ((constructor ));
void after_main(void) __attribute__((destructor));

void before_main() {
   if (!iDoneInit) {
      SONInitFiles();
      iDoneInit = 1;
   }
}

void after_main() {
   if (iDoneInit) {
      SONCleanUp();
      iDoneInit = 0;
   }
}
#endif
