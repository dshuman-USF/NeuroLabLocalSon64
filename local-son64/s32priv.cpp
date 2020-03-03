// s32priv.cpp
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

//! \file s32priv.cpp
//! \brief code to make the 32-bit son file look like a 64-bit file
/*!
A TSon32File implements the functionality of a CSon64File file, that is, it makes the
32-bit filing system look (as much as is possible) like a 64-bit file but one that has
limits on size and duration.
*/

// Stop the Microsoft compiler moaning about strncpy (which is used as safely as possible).
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "s32priv.h"
#include <assert.h>
#include <vector>
#include <cmath> // dale added -- need to include this before son.c includes math.h

//! Namespace used to contain the 32-bit son library
/*!
Some of the names in the 32-bit son library collide with those in the 64-bit library
so we put them in their own namespace to avoid problems.
*/
namespace ceds32
{
#include "son.c"
};

#include "s64filt.h"

using namespace ceds64;
using namespace ceds32;
using namespace std;

#define S32_BUFSZ 0x8000    //!< The physical size of the disk buffers to use

// Note that -1 maps to -1, so returns from PrevNTime() work
// pass int in so that when a TSTime is passed in it does not get truncated.
static int S64Err(int s32Err)
{
    switch (s32Err)
    {
    case 0: return S64_OK;
    case SON_NO_FILE: 
    case SON_BAD_HANDLE:
    case SON_OUT_OF_HANDLES:
    case SON_NO_HANDLES:    return NO_FILE;
    case SON_NO_ACCESS:     return NO_ACCESS;
    case SON_OUT_OF_MEMORY: return NO_MEMORY;
    case SON_BAD_READ:      return BAD_READ;
    case SON_BAD_WRITE:     return BAD_WRITE;
    case SON_CHANNEL_UNUSED:
    case SON_NO_CHANNEL:    return NO_CHANNEL;
    case SON_CHANNEL_USED:  return CHANNEL_USED;
    case SON_PAST_EOF:      return PAST_EOF;
    case SON_WRONG_FILE:    return WRONG_FILE;
    case SON_NO_EXTRA:      return NO_EXTRA;
    case SON_CORRUPT_FILE:  return CORRUPT_FILE;
    case SON_PAST_SOF:      return PAST_SOF;
    case SON_READ_ONLY:     return READ_ONLY;
    case SON_BAD_PARAM:     return BAD_PARAM;
    default:                return s32Err;      // so +ve stays +ve
    }
}

// The channel types map exactly, so this is just a cast
static ceds64::TDataKind Kind64(ceds32::TDataKind kind32)
{
    return static_cast<ceds64::TDataKind>(kind32);
}

static ceds32::TDataKind Kind32(ceds64::TDataKind kind64)
{
    return static_cast<ceds32::TDataKind>(kind64);
}

// Create a matching filter mask and return a pointer to it.
// pFM64    Input filter mask. If not set, then we return nullptr
// fm       Where we create the equivalent mask
// This code assumes that the memory layout of the masks is the same. The new
// filters use 32-bit ints at the bottom to speed things up. The old uses bytes.
// These will layout the same in a little-endian system (which is all we use) but
// if you ever use big-endian there may be a problem.
static TpFilterMask S32FM(const CSFilter* pFM64, TFilterMask &fm)
{
    if (!pFM64)
        return nullptr;

    for (int i=0; i<4; ++i)     // copy the layers of the mask we can use
        pFM64->GetElements(fm.aMask[i], i);
    fm.lFlags = (pFM64->GetMode() == CSFilter::eM_and) ? SON_FMASK_ANDMODE : SON_FMASK_ORMODE;
    fm.nTrace = pFM64->GetColumn();
    return &fm;
}

//! Son32 constructor
/*!
\param iBig This parameter determines what type of 32-bit file the Create() call generates.
            Set -1 for unspecified, 0 for a small format file (2 GB limit) or 1 for a big
            file (1 TB limit). If iBig is < 1, a small format file is generated. It has no
            impact on the Open() call.
*/
TSon32File::TSon32File(int iBig)
    : m_fh( -1 )
    , m_dBufferedSecs( 0.0 )
    , m_iCreateBig( iBig )
{
}


//! Son32 file destructor
/*!
If there is an open data file associated with the TSon32File object, the Close()
function is called to tidy it up, then all resources associated with the file are
released.
*/
TSon32File::~TSon32File()
{
    if (m_fh >= 0)          // MUST NOT call close on an undefined handle
        Close();
}

#ifdef _UNICODE
//! Convert a UTF8 string to a wide string
static std::wstring s2ws(const std::string& name)
{
    std::wstring wStr;
    int nIn = (int)name.length();
    if (nIn)          // If got input string
    {
        // As we 'know' that the UTF-16 representation of anything in UTF-8 can take
        // no more TCHARs that the source, allocate enough space.
        const int nOut = nIn + 1;
        std::vector<wchar_t> vw(nOut);
        // name is UNICODE string so we convert UTF8 to Wide chars
        int newLen = MultiByteToWideChar(
            CP_UTF8, 0,
            name.data(), nIn, vw.data(), nOut
            );
        vw.data()[newLen] = 0;    // terminate the string
        wStr = vw.data();
    }
    return wStr;
}

int TSon32File::Create(const wchar_t* szName, uint16_t nChannels, uint32_t nFUser)
{
    if ((nFUser > 65535) || (nChannels > SONABSMAXCHANS))
        return BAD_PARAM;
    if (m_fh >= 0)
        return CHANNEL_USED;
    m_fh = SONCreateFileEx(szName, nChannels, (WORD)nFUser, m_iCreateBig > 0);
    if (m_fh >= 0)
        SONExtMarkAlign(m_fh, 1);   // aligned external markers
    return m_fh<0 ? S64Err(m_fh) : S64_OK;
}

int TSon32File::Open(const wchar_t* szName, int iOpenMode, int flags)
{
    if (iOpenMode<0)                // translate try readwrite, accept readonly
        iOpenMode = 2;
    m_fh = SONOpenOldFile(szName, iOpenMode);
    return m_fh<0 ? S64Err(m_fh) : S64_OK;
}

#endif

//------------------------------------------------------------------------------

int TSon32File::Create(const char* szName, uint16_t nChannels, uint32_t nFUser)
{
#ifdef _UNICODE
    return Create(s2ws(szName).c_str(), nChannels, nFUser);
#else
    if ((nFUser > 65535) || (nChannels > SONABSMAXCHANS))
        return BAD_PARAM;
    if (m_fh >= 0)
        return CHANNEL_USED;
    m_fh = SONCreateFileEx(szName, nChannels, (WORD)nFUser, m_iCreateBig > 0);
    if (m_fh >= 0)
        SONExtMarkAlign(m_fh, 1);   // aligned external markers
    return m_fh<0 ? S64Err(m_fh) : S64_OK;
#endif
}

int TSon32File::Open(const char* szName, int iOpenMode, int flags)
{
#ifdef _UNICODE
    return Open(s2ws(szName).c_str(), iOpenMode, flags);
#else
    if (iOpenMode<0)                // translate try readwrite, accept readonly
        iOpenMode = 2;
    m_fh = SONOpenOldFile(szName, iOpenMode);
    return m_fh<0 ? S64Err(m_fh) : S64_OK;
#endif
}

int TSon32File::EmptyFile()
{
    if (m_fh >= 0)
        return S64Err(SONEmptyFile(m_fh));
    else
        return NO_FILE;
}

bool TSon32File::CanWrite() const
{
    return SONCanWrite(m_fh) != 0;
}

int TSon32File::Close()
{
    if (m_fh >= 0)
    {
        short err = SONCloseFile(m_fh);
        m_fh = -1;                      // always say it is closed
        return S64Err(err);
    }
    else
        return NO_FILE;
}

int TSon32File::GetFreeChan() const
{
    return S64Err(SONGetFreeChan(m_fh));
}

bool TSon32File::IsModified() const
{
    return SONUpdateHeader(m_fh) > 0;
}

int TSon32File::Commit(int iFlags)
{
    int flags = ((iFlags & eCF_flushSys) ? SON_CF_FLUSHSYS : 0) |
                ((iFlags & eCF_headerOnly) ? SON_CF_HEADERONLY : 0);
    return S64Err( SONCommitFileEx(m_fh, flags) );
}

int TSon32File::FlushSysBuffers()
{
    return S64_OK;
}

double TSon32File::GetTimeBase() const
{
    return SONGetusPerTime(m_fh)*SONTimeBase(m_fh, 0.0);
}

void TSon32File::SetTimeBase(double dSecPerTick)
{
    SONSetFileClock(m_fh, 1, 1);
    SONTimeBase(m_fh, dSecPerTick);
}

int TSon32File::SetExtraData(const void* pData, uint32_t nBytes, uint32_t nOffset)
{
    if ((nBytes > 0xffff) || (nOffset > 0xffff))
        return BAD_PARAM;
    return S64Err(SONGetExtraData(m_fh, const_cast<void*>(pData), (WORD)nBytes, (WORD)nOffset, true));
}

int TSon32File::GetExtraData(void* pData, uint32_t nBytes, uint32_t nOffset)
{
    if ((nBytes > 0xffff) || (nOffset > 0xffff))
        return BAD_PARAM;
    return S64Err(SONGetExtraData(m_fh, pData, (WORD)nBytes, (WORD)nOffset, FALSE));
}

uint32_t TSon32File::GetExtraDataSize() const
{
    return static_cast<uint32_t>(SONGetExtraDataSize(m_fh));
}

//! Truncate (if needed) a UTF8 string to fit in the desired space
static void LimitUTF8String(string& s, size_t nMax)
{
    if (s.size() <= nMax)       // if all is well...
        return;                 // ...nothing to be done
    if ((s[nMax - 1] & 0xc0) == 0x80)   // If first deleted is a trail code
    {
        // We must delete the entire UTF8 character. Remove trail codes
        --nMax;
        while ((nMax > 0) && ((s[nMax - 1] & 0xc0) == 0x80))
            --nMax;
        if ((nMax > 0) && ((s[nMax - 1] & 0xc0) == 0xc0))
            --nMax;
        s.resize(nMax);
    }
}

// The new system allows more comments - these are silently ignored
int TSon32File::SetFileComment(int n, const char* szComment)
{
    if ((n > NUMFILECOMMENTS) || (n < 0))
        return BAD_PARAM;
    if (n < SON_NUMFILECOMMENTS)
    {
        if (strlen(szComment) > SON_COMMENTSZ)
        {
            string s(szComment);
            LimitUTF8String(s, SON_COMMENTSZ);
            SONSetFileComment(m_fh, (WORD)n, s.c_str());
        }
        else
            SONSetFileComment(m_fh, (WORD)n, szComment);
    }
    return S64_OK;
}

int TSon32File::GetFileComment(int n, int nSz, char* szComment) const
{
    if (m_fh < 0)
        return NO_FILE;
    if ((n > NUMFILECOMMENTS) || (n < 0))
        return BAD_PARAM;
    if (szComment && (nSz > 0))     // If we are returning something
    {
        if (n < SON_NUMFILECOMMENTS)
            SONGetFileComment(m_fh, (WORD)n, szComment, nSz - 1);
        else
            szComment[0] = 0;       // empty string if not supported
    }
    return SON_COMMENTSZ + 1;   // space needed for copy
}

int TSon32File::MaxChans() const
{
    return S64Err(SONMaxChans(m_fh));
}

// We know that ceds64::TCreator is bit for bit equivalent to ceds32::TSONCreator
int TSon32File::AppID(TCreator* pRead, const TCreator* pWrite)
{
    static_assert(sizeof(TCreator) == sizeof(TSONCreator), "TCreator size mismatch");
    return S64Err(SONAppID(m_fh,
        reinterpret_cast<ceds32::TSONCreator*>(pRead),
        reinterpret_cast<const ceds32::TSONCreator*>(pWrite)));
}

// The ceds64 and ceds32 TTimeDate TSONTimeDate structures are bitwise identical
// Return 1 if OK, 0 if null, -ve for bad
int TSon32File::TimeDate(TTimeDate* pTDGet, const TTimeDate* pTDSet)
{
    static_assert(sizeof(TTimeDate) == sizeof(TSONTimeDate), "TTimeDate size mismatch");
    return S64Err(SONTimeDate(m_fh,
        reinterpret_cast<ceds32::TSONTimeDate*>(pTDGet),
        reinterpret_cast<const ceds32::TSONTimeDate*>(pTDSet)));
}

// version numbers for the new files start at major*256 and the first major number is 1.
// Old files will have version numbers up to 9, so are easily distinguished.
int TSon32File::GetVersion() const
{
    return S64Err(SONGetVersion(m_fh));
}

uint64_t TSon32File::FileSize() const
{
    return static_cast<uint64_t>(SONFileSizeD(m_fh));
}

uint64_t TSon32File::ChanBytes(TChanNum chan) const
{
    return static_cast<uint64_t>(SONChanBytes(m_fh, chan));
}

TSTime64 TSon32File::MaxTime(bool /*bReadChans*/) const
{
    return S64Err(SONMaxTime(m_fh));
}

void TSon32File::ExtendMaxTime(TSTime64 t)
{
    SONExtendMaxTime(m_fh, (t < TSTIME_MAX) ? static_cast<TSTime>(t) : TSTIME_MAX);
}

ceds64::TDataKind TSon32File::ChanKind(TChanNum chan) const
{
    return Kind64(SONChanKind(m_fh, (WORD)chan));
}

TSTime64 TSon32File::ChanDivide(TChanNum chan) const
{
    return SONChanDivide(m_fh, (WORD)chan);
}

double TSon32File::IdealRate(TChanNum chan, double dRate)
{
    return SONIdealRate(m_fh, (WORD)chan, (float)dRate);
}

int TSon32File::PhyChan(TChanNum chan) const
{
    return S64Err(SONPhyChan(m_fh, (WORD)chan));
}

int TSon32File::SetChanComment(TChanNum chan, const char* szComment)
{
    if (m_fh < 0)
        return NO_FILE;
    if (strlen(szComment) > SON_COMMENTSZ)
    {
        string s(szComment);
        LimitUTF8String(s, SON_COMMENTSZ);
        SONSetChanComment(m_fh, (WORD)chan, s.c_str());
    }
    else
        SONSetChanComment(m_fh, (WORD)chan, szComment);
    return S64_OK;
}

int TSon32File::GetChanComment(TChanNum chan, int nSz, char* szComment) const
{
    if (m_fh < 0)
        return NO_FILE;
    if (szComment && (nSz > 0))
    {
        char work[SON_CHANCOMSZ + 1];
        work[0] = 0;
        SONGetChanComment(m_fh, (WORD)chan, work, SON_CHANCOMSZ);
        strncpy(szComment, work, nSz);
        szComment[nSz - 1] = 0;
    }
    return SON_CHANCOMSZ + 1;
}

int TSon32File::SetChanTitle(TChanNum chan, const char* szTitle)
{
    if (m_fh < 0)
        return NO_FILE;
    if (strlen(szTitle) > SON_TITLESZ)
    {
        string s(szTitle);
        LimitUTF8String(s, SON_TITLESZ);
        SONSetChanTitle(m_fh, (WORD)chan, s.c_str());
    }
    else
        SONSetChanTitle(m_fh, (WORD)chan, szTitle);
    return S64_OK;
}

int TSon32File::GetChanTitle(TChanNum chan, int nSz, char* szTitle) const
{
    if (m_fh < 0)
        return NO_FILE;
    if (szTitle && (nSz > 0))
    {
        char work[SON_TITLESZ + 1];
        work[0] = 0;
        SONGetChanTitle(m_fh, (WORD)chan, work);
        strncpy(szTitle, work, nSz);
        szTitle[nSz - 1] = 0;
    }
    return SON_TITLESZ + 1;
}

int TSon32File::SetChanScale(TChanNum chan, double dScale)
{
    if (m_fh < 0)
        return NO_FILE;
    SONSetADCScale(m_fh, (WORD)chan, (float)dScale);
    return S64_OK;
}

int TSon32File::GetChanScale(TChanNum chan, double& dScale) const
{
    if (m_fh < 0)
        return NO_FILE;

    // Not all channel types have scale and offset
    auto kind = SONChanKind(m_fh, chan);
    if ((kind == Adc) || (kind == RealWave) || (kind == AdcMark))
    {
        float fScale;
        SONGetADCInfo(m_fh, (WORD)chan, &fScale, nullptr, nullptr, nullptr, nullptr);
        dScale = fScale;
    }
    else
        dScale = 1.0;
    return S64_OK;
}

int TSon32File::SetChanOffset(TChanNum chan, double dOffset)
{
    if (m_fh < 0)
        return NO_FILE;
    SONSetADCOffset(m_fh, (WORD)chan, (float)dOffset);
    return S64_OK;
}

int TSon32File::GetChanOffset(TChanNum chan, double& dOffset) const
{
    if (m_fh < 0)
        return NO_FILE;

    // Not all channel types have scale and offset
    auto kind = SONChanKind(m_fh, chan);
    if ((kind == Adc) || (kind == RealWave) || (kind == AdcMark))
    {
        float fOffset;
        SONGetADCInfo(m_fh, (WORD)chan, nullptr, &fOffset, nullptr, nullptr, nullptr);
        dOffset = fOffset;
    }
    else
        dOffset = 0.0;
    return S64_OK;
}

int TSon32File::SetChanUnits(TChanNum chan, const char* szUnits)
{
    if (m_fh < 0)
        return NO_FILE;
    if (strlen(szUnits) > SON_UNITSZ)
    {
        string s(szUnits);
        LimitUTF8String(s, SON_UNITSZ);
        SONSetADCUnits(m_fh, (WORD)chan, s.c_str());
    }
    else
        SONSetADCUnits(m_fh, (WORD)chan, szUnits);
    return S64_OK;
}

int TSon32File::GetChanUnits(TChanNum chan, int nSz, char* szUnits) const
{
    if (m_fh < 0)
        return NO_FILE;
    if (szUnits && (nSz > 0))
    {
        char work[SON_UNITSZ + 1];
        work[0] = 0;
        auto kind = SONChanKind(m_fh, chan);
        if ((kind == Adc) || (kind == AdcMark) || (kind == RealWave))
            SONGetADCInfo(m_fh, (WORD)chan, nullptr, nullptr, work, nullptr, nullptr);
        else
            SONGetExtMarkInfo(m_fh, (WORD)chan, work, nullptr, nullptr);
        strncpy(szUnits, work, nSz);
        szUnits[nSz - 1] = 0;
    }
    return SON_UNITSZ + 1;  // Space needed to read the entire string
}


TSTime64 TSon32File::ChanMaxTime(TChanNum chan) const
{
    return SONChanMaxTime(m_fh, (WORD)chan);
}

TSTime64 TSon32File::PrevNTime(TChanNum chan, TSTime64 tStart, TSTime64 tEnd, uint32_t n, const CSFilter* pFilter, bool bAsWave)
{
    if ((tStart > TSTIME_MAX) || (tEnd > TSTIME_MAX))
        return PAST_EOF;
    TFilterMask fm;
    TpFilterMask pFM = S32FM(pFilter, fm);  // convert filter pointer
    return SONLastPointsTime(m_fh, (WORD)chan, (TSTime)tStart, (TSTime)tEnd, (long)n, bAsWave, pFM);
}

int TSon32File::ChanDelete(TChanNum chan)
{
    return S64Err(SONChanDelete(m_fh, (WORD)chan));
}

//! A 32-bit file can be asked if a channel is deleted but does not know the type
/*!
If you ask to undelete, the response is CHANNEL_TYPE error as 32-bit files cannot
be undeleted. There is a script UnDelCh.s2s that can do it, but the file will need
to be recovered after running it (and it can only guess at the channel type).
\return If we detect a deleted channel, we return the type as Adc (we have no idea),
        otherwise a negative error code.
*/
int TSon32File::ChanUndelete(TChanNum chan, eCU action)
{
    if (action == eCU_kind)
    {
        if (!SONChanPnt(m_fh, chan))
            return NO_CHANNEL;

        if ((SONChanKind(m_fh, chan) == ChanOff) &&
            (SONDelBlocks(m_fh, chan) > 0))
            return Adc;
        else
            return ChanOff;
    }
    return CHANNEL_TYPE;
}

int TSon32File::GetChanYRange(TChanNum chan, double& dLow, double& dHigh) const
{
    if (m_fh < 0)
        return NO_FILE;
    float fLow = 0.0, fHigh = 0.0;
    SONYRange(m_fh, (WORD)chan, &fLow, &fHigh);
    dLow = fLow;
    dHigh = fHigh;
    return S64_OK;
}

int TSon32File::SetChanYRange(TChanNum chan, double dLow, double dHigh)
{
    return S64Err(SONYRangeSet(m_fh, (WORD)chan, (float)dLow, (float)dHigh));
}

// This is likely to be a tricky one! If the channel is a waveform, then the sizes
// are as reported. Otherwise the sizes will be larger. Events and markers are easy.
// TextMark data is larger by the Marker size increase, plus rounding up the size
// to a multiple of 8. We are only trying to support simple data arrangements found
// in existing data files.
int TSon32File::ItemSize(TChanNum chan) const
{
    if (m_fh < 0)
        return NO_FILE;
    ceds64::TDataKind kind = ChanKind(chan);    // see what we have
    int nBytes = SONItemSize(m_fh, (WORD)chan); // result is 1 if bad call
    if (nBytes < 2)                             // if unknown result or off...
        return 0;                               // ...return
    switch (kind)
    {
    case ChanOff: return 0;
    case Adc:     return sizeof(short);
    case EventFall:
    case EventRise: return sizeof(TSTime64);
    case Marker: return sizeof(TMarker);
    case RealWave: return sizeof(float);
    default:
        nBytes += sizeof(ceds64::TMarker)-sizeof(ceds32::TMarker);
        nBytes = ((nBytes + 7) >> 3) << 3;      // round up to multiple of 8 bytes
        return nBytes;
    }
}

//------------------------- Sample Saving and Buffering ---------------------------

//! Set the save/no save state for use when sampling.
/*
\param chan Either a single channel number or a negative number for all channels.
\param t    The time at which the change is to start.
\param bSave The new state (true for saving, false for not saving)
*/
void TSon32File::Save(int chan, TSTime64 t, bool bSave)
{
    if (t <= TSTIME_MAX)
        SONSave(m_fh, chan, (TSTime)t, bSave);
}

//! Set the Save/no save state for a time range
/*
\param chan Either a single channel number or a negative number for all channels.
\param tFrom The time at which saving is to start
\param tUpto The time at which we stop saving (unless background was already saving)
*/
void TSon32File::SaveRange(int chan, TSTime64 tFrom, TSTime64 tUpto)
{
    if (tFrom <= TSTIME_MAX)
    {
        --tUpto;                    // as times are inclusive in 32-bits system
        if (tUpto > TSTIME_MAX)
            tUpto = TSTIME_MAX;
        SONSaveRange(m_fh, chan, (TSTime)tFrom, (TSTime)tUpto);
    }
}

// Get a list of NoSave/Save times, see CSon64File doc
// We do the best we can to emulate the 64-bit library with the 32-bit library.
int TSon32File::NoSaveList(TChanNum chan, TSTime64* pTimes, int nMax, TSTime64 tFrom, TSTime64 tUpto) const
{
    TSTime aTimes[CHANGES];
    int nGot = S64Err(SONNoSaveList(m_fh, chan, aTimes));
    if (nGot <= 0)                  // if no times to be found...
        return 0;                   // ...we are done

    // Find first returned time that is within our range
    int index = 0;
    while ((index < nGot) && (aTimes[index] < tFrom))
        ++index;

    // We are either off the end, or we have a time past the start
    if (index >= nGot)
    {
        if ((nGot & 1) == 0)        // If current state is saving...
            return 0;               // ...then we are done
        if (--nMax >= 0)            // If not saving, but past the end...
            *pTimes++ = tFrom;      // ...say this range is all not saving
        return 1;                   // and we are done
    }

    int nRet = 0;                   // the number we would like to return
    if (index & 1)                  // If index item is a turn on...
    {
        if (aTimes[index] == tFrom) // ...if this time is at tFrom
            ++index;                // ...then skip it as next is an off
        else
        {                           // ...then we were off at tFrom...
            if (--nMax >= 0)        // ...do, if output space, ...
                *pTimes++ = tFrom;  // ...set off at the very start
            nRet = 1;               // we have 1 item
        }
    }

    // Add any additional changes
    while ((index < nGot) && (aTimes[index] < tUpto))
    {
        if (--nMax >= 0)            // if output space
            *pTimes++ = aTimes[index];
        ++nRet;
        ++index;
    }

    return nRet;
}

// This does not work correctly for a 32-bit file, but we probably should never be calling it!
bool TSon32File::IsSaving(TChanNum chan, TSTime64 /*tAt*/) const
{
    return SONIsSaving(m_fh, (WORD)chan) != 0;
}

int TSon32File::LatestTime(int chan, TSTime64 t)
{
    if (t > TSTIME_MAX)
        return PAST_EOF;
    return S64Err(SONLatestTime(m_fh, chan, TSTime(t)));
}

//! FORCE_BUFFERING defined means that we always set some channel buffering.
/*!
There was a bug in SONWriteBlock (now fixed) that meant that if there was no
buffering, the channel read buffer held data read before adding new data,
causing new data to be unreadable.

In any case, having some buffering is preferable for performance reasons. GPS 22/Aug/13
*/
#define FORCE_BUFFERING

// This is a problem as the old system does not have a time-based way to do this.
// For now just return 0 if OK or a negative code.
double TSon32File::SetBuffering(int chan, size_t nBytes, double dSeconds) 
{
    if (!CanWrite())
        return 0.0;

    if (dSeconds < 0.0)         // If we are to use the saved buffering time...
        dSeconds = m_dBufferedSecs;

    if (chan >= 0)
    {
        if (ChanKind(chan) == ChanOff)
            return NO_CHANNEL;
        size_t nObjSize = (size_t)ItemSize(chan);  // bytes per item
        if (dSeconds > 0)
        {
            double dIdealRate = IdealRate(chan);
            double dSpace = nObjSize * dIdealRate * dSeconds; // predicted space
            if ((nBytes == 0) || (dSpace < nBytes))
                nBytes = static_cast<size_t>(dSpace);
            dSeconds = (dIdealRate > 0) ? nBytes / (dIdealRate*nObjSize) : 0;
        }
#ifdef FORCE_BUFFERING
        else if (nBytes == 0)       // BUGBUG: To test if lack of buffering causes a problem
            nBytes = 32768;
#endif
        // This just sets the desired size, SONSetBuffering() does the dirty work
        SONSetBuffering(m_fh, chan, (int)nBytes);
    }
    else
    {
        const int nMaxChans = MaxChans();       // get maximum channels in the file
        double dBytesPerSec = 0.0;              // initialize the total bytes per sec
        for (int i = 0; i<nMaxChans; ++i)
        {
            if (ChanKind(i) != ChanOff)
                dBytesPerSec += ItemSize(i)*IdealRate(i);
        }

        // If this needs too much space, or no seconds given, calc equivalent seconds.
        if ((dSeconds <= 0.0) ||                // if no time supplied, or...
            (dBytesPerSec*dSeconds > nBytes))   // ...too much space needed, then...
        {
#ifdef FORCE_BUFFERING
            if (nBytes == 0)
                nBytes = 1000000;               // BUGBUG: to check if lack of buffering is a problem
#endif
            dSeconds = nBytes / dBytesPerSec;   // ...scale time to available space
        }

        for (int i = 0; i<nMaxChans; ++i)
        {
            if (ChanKind(i) != ChanOff)
                SONSetBuffering(m_fh, i, (int)(IdealRate(i)*dSeconds));
        }

        m_dBufferedSecs = dSeconds;             // Save buffering time
    }

    SONSetBuffSpace(m_fh);
    return dSeconds;
}

//------------------------ data transfer and channel definition ------------------------
//------------------------- Event channels ---------------------------------
int TSon32File::SetEventChan(TChanNum chan, double dRate, ceds64::TDataKind evtKind, int iPhyCh)
{
    int err = SONSetEventChan(m_fh, (WORD)chan, (short)iPhyCh, S32_BUFSZ, "", "",(float)dRate, Kind32(evtKind));
    return S64Err(err);
}

// This only writes data that is within the time range
int TSon32File::WriteEvents(TChanNum chan, const TSTime64* pData, size_t count)
{
    if (count == 0)
        return 0;
    vector<TSTime> vt(count);
    for (size_t i = 0; i<count; ++i)
    {
        if (pData[i] <= TSTIME_MAX)
            vt[i] = (TSTime)pData[i];   // copy the data
        else
        {
            count = i;
            if (i == 0)                 // if all data beyond the end...
                return PAST_EOF;        // ...flag as an error
            break;
        }
    }
    return S64Err(SONWriteEventBlock(m_fh, (WORD)chan, &vt[0], (long)count));
}

int TSon32File::ReadEvents(TChanNum chan, TSTime64* pData, int nMax, TSTime64 tFrom, TSTime64 tUpto, const CSFilter* pFilter)
{
    if ((tFrom > TSTIME_MAX) ||         // see if no data is possible
        (nMax <= 0))
        return 0;
    TSTime eTime = (tUpto > TSTIME_MAX) ? TSTIME_MAX : (TSTime)(tUpto-1);
    TFilterMask fm;
    TpFilterMask pFM = S32FM(pFilter, fm);
    vector<TSTime> vt(nMax);            // space to read into
    int n = SONGetEventData(m_fh, (WORD)chan, &vt[0], nMax, (TSTime)tFrom, eTime, nullptr, pFM);
    if (n >= 0)
    {
        for (int i=0; i<n; ++i)
            pData[i] = vt[i];
    }
    else
        n = S64Err(n);
    return n;
}

//------------------------------------ Marker channels ----------------------------------
int TSon32File::SetMarkerChan(TChanNum chan, double dRate, ceds64::TDataKind kind, int iPhyChan)
{
    return SetEventChan(chan, dRate, kind, iPhyChan);   // Markers are same as events
}

int TSon32File::WriteMarkers(TChanNum chan, const ceds64::TMarker* pData, size_t count)
{
    if (count == 0)
        return 0;

    // If user writes EventBoth we have to assume that they have got the first point
    // and the alternation of points right (we could check with more effort)
    if (ChanKind(chan) == EventBoth)
    {
        vector<TSTime> vt(count);       // to hold converted times
        for (size_t i=0; i<count; ++i)
        {
            if (pData[i].m_time <= TSTIME_MAX)
                vt[i] = (TSTime)pData[i].m_time;
            else
            {
                if (i == 0)             // if nothing in time range...
                    return PAST_EOF;    // ...then this is an error
                count = (size_t)i;
                break;
            }
        }
        return S64Err(SONWriteEventBlock(m_fh, (WORD)chan, &vt[0], (long)count));
    }
    else
    {
        vector<ceds32::TMarker> vm(count);  // space to hold our markers
        for (size_t i = 0; i<count; ++i)
        {
            if (pData[i] <= TSTIME_MAX)     // check in range...
            {
                vm[i].mark = (TSTime)pData[i];  // modify the data
                for (int j=0; j<4; ++j)
                    vm[i].mvals[j] = pData[i].m_code[j];
            }
            else
            {
                if (i == 0)             // if nothing in time range...
                    return PAST_EOF;    // ...then this is an error
                count = (size_t)i;      // else only copy in time range stuff
                break;
            }
        }
        return S64Err(SONWriteMarkBlock(m_fh, (WORD)chan, &vm[0], (long)count));
    }
}

int TSon32File::ReadMarkers(TChanNum chan, ceds64::TMarker* pData, int nMax, TSTime64 tFrom, TSTime64 tUpto, const CSFilter* pFilter)
{
    if ((tFrom > TSTIME_MAX) ||         // see if no data is possible
        (nMax <= 0))
        return 0;

    TSTime eTime = (tUpto > TSTIME_MAX) ? TSTIME_MAX : (TSTime)(tUpto-1);
    TFilterMask fm;
    TpFilterMask pFM = S32FM(pFilter, fm);

    // We could be cute and use the second half of pData to save allocating more
    // memory and releasing it. We can return to this once things are working. This
    // applied to all the read routines. Do not do for write as the buffers are const.
    int n;
    if (ChanKind(chan) == EventBoth)
    {
        vector<TSTime> vt(nMax);            // work space for read
        BOOLEAN bLevLow;
        n = SONGetEventData(m_fh, (WORD)chan, &vt[0], (long)nMax, (TSTime)tFrom, eTime, &bLevLow, pFM);
        if (n > 0)
        {
            bool b = bLevLow != 0;
            if (pFilter)
            {
                int nRead = n;
                n = 0;
                for (int i = 0; i<nRead; ++i)    // convert marker type
                {
                    pData[n].m_time = vt[i];    // the time
                    pData[n].m_int[0] = 0;
                    pData[n].m_int[1] = 0;
                    pData[n].m_code[0] = b;
                    b = !b;
                    if (pFilter->Filter(pData[n]))
                        ++n;
                }
            }
            else
            {
                for (int i = 0; i<n; ++i)    // convert marker type
                {
                    pData[i].m_time = vt[i];    // the time
                    pData[i].m_int[0] = 0;
                    pData[i].m_int[1] = 0;
                    pData[i].m_code[0] = b;
                    b = !b;
                }
            }
        }
    }
    else
    {
        vector<ceds32::TMarker> vm(nMax);   // space to read into
        n = SONGetMarkData(m_fh, (WORD)chan, &vm[0], (long)nMax, (TSTime)tFrom, eTime, pFM);
        if (n > 0)
        {
            for (int i = 0; i<n; ++i)       // convert marker type
            {
                pData[i].m_time = vm[i].mark;   // the time
                for (int j=0; j<4; ++j)
                    pData[i].m_code[j] = vm[i].mvals[j];
                pData[i].m_int[1] = 0;
            }
        }
    }
    return S64Err(n);
}

int TSon32File::EditMarker(TChanNum chan, TSTime64 t, const ceds64::TMarker* pM, size_t nCopy)
{
    if (t > TSTIME_MAX)
        return 0;
    WORD wCopy;
    if (nCopy < sizeof(TSTime64))
        return BAD_PARAM;
    int nSize32 = SONItemSize(m_fh, chan);  // size of item on disk (may not align nicely)
    if (nSize32 < 0)
        return S64Err(nSize32);             // probably a stupid channel

    // The 64-bit sizes are always rounded up to a multiple of 8, but the 32-bit ones are not
    // so we may end up with more data than we can write, so reduce to fit
    if (nCopy > nSize32 + sizeof(TMarker) - sizeof(ceds32::TMarker))
        nCopy = nSize32 + sizeof(TMarker) - sizeof(ceds32::TMarker);

    wCopy = sizeof(TSTime);                 // assume just the time
    if (nCopy > sizeof(TMarker))            // If includes attached data...
        wCopy = WORD(sizeof(ceds32::TMarker) + nCopy - sizeof(TMarker));
    else if (nCopy > sizeof(TSTime64))      // If no attached, but includes marker codes...
        wCopy = WORD((nCopy-sizeof(TSTime64) > 4) ? 4 + sizeof(TSTime) : nCopy-sizeof(TSTime64) + sizeof(TSTime));
    else                                    // else is just...
        wCopy = 4;                          // ...the time stamp

    // Now convert the marker to a Son32 marker
    size_t nUse = max(size_t(wCopy), sizeof(ceds32::TMarker));
    vector<char> buff(nUse);        // make some space and a nasty hack
    ceds32::TMarker* pM32 = reinterpret_cast<ceds32::TMarker*>(&buff[0]);
    pM32->mark = static_cast<TSTime>(pM->m_time);
    for (int i= 0; i<4; ++i)
        pM32->mvals[i] = pM->m_code[i];
    if (nCopy > sizeof(TMarker))
        memcpy(pM32+1, pM+1, nCopy - sizeof(TMarker));

    return S64Err(SONSetMarker(m_fh, chan, (TSTime)t, pM32, wCopy));
}

//----------------------------- event both --------------------------------------------
int TSon32File::SetLevelChan(TChanNum chan, double dRate, int iPhyChan)
{
    return SetEventChan(chan, dRate, EventBoth, iPhyChan);
}

int TSon32File::SetInitLevel(TChanNum chan, bool bLevel)
{
    if (m_fh < 0)
        return NO_FILE;

    SONSetInitLow(m_fh, (WORD)chan, !bLevel);
    return S64_OK;
}

int TSon32File::WriteLevels(TChanNum chan, const TSTime64* pData, size_t count)
{
    return WriteEvents(chan, pData, count);
}

int TSon32File::ReadLevels(TChanNum chan, TSTime64* pData, int nMax, TSTime64 tFrom, TSTime64 tUpto, bool& bLevel)
{
    if ((tFrom > TSTIME_MAX) ||         // see if no data is possible
        (nMax <= 0))
        return 0;

    TSTime eTime = (tUpto > TSTIME_MAX) ? TSTIME_MAX : (TSTime)(tUpto-1);
    vector<TSTime> vt(nMax);            // work space for read
    BOOLEAN bLevLow;
    int n = SONGetEventData(m_fh, (WORD)chan, &vt[0], (long)nMax, (TSTime)tFrom, eTime, &bLevLow, nullptr);
    for (int i=0; i<n; ++i)
        pData[i] = vt[i];
    bLevel = bLevLow != 0;
    return S64Err(n);
}

//---------------------------------- Extended marker types -----------------------------------------

int TSon32File::SetTextMarkChan(TChanNum chan, double dRate, size_t nMax, int iPhyChan)
{
    return S64Err(SONSetTextMarkChan(m_fh, (WORD)chan, (short)iPhyChan, S32_BUFSZ, "", "", (float)dRate, "", (WORD)nMax));
}

int TSon32File::SetExtMarkChan(TChanNum chan, double dRate, ceds64::TDataKind kind, size_t nRows, size_t nCols, int iPhyChan, TSTime64 tDvd, int nPre)
{
    if ((tDvd > TSTIME_MAX) || (nRows > 65535) || (nCols > 65535) || (nPre > 32767))
        return BAD_PARAM;

    int err;
    switch (kind)
    {
    case Marker:
        return SetMarkerChan(chan, dRate, Marker, iPhyChan);

    case AdcMark:
        err = SONSetWaveMarkChan(m_fh, (WORD)chan, (short)iPhyChan, (TSTime)tDvd, S32_BUFSZ,
                                 "", "", (float)dRate, 1.0, 0.0, "",
                                 (WORD)nRows, (short)nPre, (WORD)nCols);
        break;

    case RealMark:
        err = SONSetRealMarkChan(m_fh, (WORD)chan, (short)iPhyChan, S32_BUFSZ,"","",(float)dRate,
                                 -1.0, 1.0, "", (WORD)nRows);
        break;

    case TextMark:
        return SetTextMarkChan(chan, dRate, nRows, iPhyChan);

    default:
        return CHANNEL_TYPE;
    }

    return S64Err(err);
}

int TSon32File::GetExtMarkInfo(TChanNum chan, size_t *pRows, size_t* pCols) const
{
    if (m_fh < 0)
        return NO_FILE;

    WORD points = 0;
    short preTrig = 0;
    int nRet = S64Err(SONChanInterleave(m_fh, (WORD)chan));
    if (nRet >= 0)
    {
        SONGetExtMarkInfo(m_fh, (WORD)chan, nullptr, &points, &preTrig);
        if (pRows)
            *pRows = points;
        if (pCols)
            *pCols = nRet;
        nRet = preTrig;
    }
    return nRet;    
}

/*! \brief Template of an indexing function to move a pointer by a byte offset.

\tparam T   The type of the pointer to be moved on.
\param p    The pointer to move on.
\param size The size of the object pointed at.
\param n    The number of items of \a size to move on by.
\return     The updated pointer to \a T.
*/
template<typename T>
T* Index(T* p, size_t size, size_t n=1)
{
    return (T*)((char*)p + size*n);
}

// This is tricky. We assume that the size of the payload attached to the extended
// marker is the same in both the 64-bit and the 32-bit library (which it should be).
int TSon32File::WriteExtMarks(TChanNum chan, const TExtMark* pData, size_t count)
{
    if (count == 0)
        return 0;
    if (count > INT32_MAX)
        return BAD_PARAM;

    size_t nSize32 = SONItemSize(m_fh, (WORD)chan);
    size_t nSize64 = ItemSize(chan);

    vector<char> work(count*nSize32);   // space to build our result
    ceds32::TMarker* p32base = reinterpret_cast<ceds32::TMarker*>(&work[0]);
    ceds32::TMarker* p32 = p32base;
    const ceds64::TMarker* p64 = pData;
    
    for (size_t i=0; i<count; ++i)
    {
        if (p64->m_time > TSTIME_MAX)   // Stop writing at the maximum possible time
        {
            if (i == 0)                 // Flag an error...
                return PAST_EOF;        // ...if all data is past the possible end
            count = i;
            break;
        }
        p32->mark = (TSTime)p64->m_time;
        for (int j=0; j<4;++j)
            p32->mvals[j] = p64->m_code[j];
        memcpy(p32+1, p64+1, nSize32 - sizeof(ceds32::TMarker));
        p32 = Index(p32, nSize32);      // move the pointers...
        p64 = Index(p64, nSize64);      // ...to the next item
    }
    return S64Err(SONWriteExtMarkBlock(m_fh, (WORD)chan, p32base, (long)count));
}

int TSon32File::ReadExtMarks(TChanNum chan, TExtMark* pData, int nMax, TSTime64 tFrom, TSTime64 tUpto, const CSFilter* pFilter)
{
    if ((nMax <= 0) || (tFrom > TSTIME_MAX))
        return 0;
    TSTime eTime = (tUpto > TSTIME_MAX) ? TSTIME_MAX : (TSTime)(tUpto-1);
    if (nMax > INT32_MAX)
        nMax = INT32_MAX;

    size_t nSize32 = SONItemSize(m_fh, (WORD)chan);
    size_t nSize64 = ItemSize(chan);

    TFilterMask fm;
    TpFilterMask pFM = S32FM(pFilter, fm);

    vector<char> work(nMax*nSize32);   // space to build our result
    ceds32::TMarker* p32base = reinterpret_cast<ceds32::TMarker*>(&work[0]);
    ceds32::TMarker* p32 = p32base;
    ceds64::TMarker* p64 = pData;
    int n = SONGetExtMarkData(m_fh, (WORD)chan, p32, (long)nMax, (TSTime)tFrom, eTime, pFM);
    if (n > 0)
    {
        for (int i=0; i<n; ++i)
        {
            p64->m_time = p32->mark;
            for (int j=0; j<4;++j)
                p64->m_code[j] = p32->mvals[j];
            p64->m_int[1] = 0;
            memcpy(p64+1, p32+1, nSize32 - sizeof(ceds32::TMarker));
            p32 = Index(p32, nSize32);      // move the pointers...
            p64 = Index(p64, nSize64);      // ...to the next item
        }
    }

    return S64Err(n);
}

//------------------------------ waveforms ---------------------------------------
int TSon32File::SetWaveChan(TChanNum chan, TSTime64 tDvd, ceds64::TDataKind wKind, double dRate, int iPhyCh)
{
    int err;
    if (tDvd > TSTIME_MAX)
        return BAD_PARAM;
    switch (wKind)
    {
    case Adc:
        err = SONSetWaveChan(m_fh, (WORD)chan, (short)iPhyCh, (TSTime)tDvd, S32_BUFSZ, "", "", 1.0, 0.0, "");
        break;
    case RealWave:
        err = SONSetRealChan(m_fh, (WORD)chan, (short)iPhyCh, (TSTime)tDvd, S32_BUFSZ, "", "", 1.0, 0.0, "");
        break;
    default:
        return CHANNEL_TYPE; 
    }
    return S64Err(err);
}

// Limit waveform count to the possible time range.
size_t TSon32File::LimitWaveCount(TChanNum chan, TSTime64 tFrom, size_t count) const
{
    TSTime64 tDivide = ChanDivide(chan);    // get the channel divisor
    if (count && (tFrom + (count-1)*tDivide >= TSTIME_MAX))
        count = (size_t)((TSTIME_MAX - tFrom + tDivide-1) / tDivide);
    return count;
}

TSTime64 TSon32File::WriteWave(TChanNum chan, const short* pData, size_t count, TSTime64 tFrom)
{
    if (tFrom > TSTIME_MAX)
        return PAST_EOF;
    count = LimitWaveCount(chan, tFrom, count);

    // The 'next write' time can come back at up to 0x7fffffff+ChanDivide()
    // so negative return times are not necessarily an error.
    uint32_t t = static_cast<uint32_t>(SONWriteADCBlock(m_fh, (WORD)chan, const_cast<short*>(pData), (long)count, (TSTime)tFrom));
    return t > INT32_MAX+ChanDivide(chan) ? (TSTime64)S64Err(static_cast<int32_t>(t)) : (TSTime64)t;
}

TSTime64 TSon32File::WriteWave(TChanNum chan, const float* pData, size_t count, TSTime64 tFrom)
{
    if (tFrom > TSTIME_MAX)
        return PAST_EOF;
    count = LimitWaveCount(chan, tFrom, count);
    // The 'next write' time can come back at up to 0x7fffffff+ChanDivide()
    // so negative return times are not necessarily an error.
    uint32_t t = static_cast<uint32_t>(SONWriteRealBlock(m_fh, (WORD)chan, const_cast<float*>(pData), (long)count, (TSTime)tFrom));
    return t > INT32_MAX + ChanDivide(chan) ? (TSTime64)S64Err(static_cast<int32_t>(t)) : (TSTime64)t;
}

int TSon32File::ReadWave(TChanNum chan, short* pData, int nMax, TSTime64 tFrom, TSTime64 tUpto, TSTime64& tFirst, const CSFilter* pFilter)
{
    if ((tFrom > TSTIME_MAX) || (nMax <= 0))
        return 0;

    TFilterMask fm;
    TpFilterMask pFM = S32FM(pFilter, fm);

    TSTime eTime = (tUpto > TSTIME_MAX) ? TSTIME_MAX : (TSTime)(tUpto-1);
    TSTime first;
    int n = SONGetADCData(m_fh, (WORD)chan, pData, (long)nMax, (TSTime)tFrom, eTime, &first, pFM);
    if (n > 0)
        tFirst = first;
    return S64Err(n);
}

int TSon32File::ReadWave(TChanNum chan, float* pData, int nMax, TSTime64 tFrom, TSTime64 tUpto, TSTime64& tFirst, const CSFilter* pFilter)
{
    if ((tFrom > TSTIME_MAX) || (nMax <= 0))
        return 0;

    TFilterMask fm;
    TpFilterMask pFM = S32FM(pFilter, fm);

    TSTime eTime = (tUpto > TSTIME_MAX) ? TSTIME_MAX : (TSTime)(tUpto-1);
    TSTime first;
    int n = SONGetRealData(m_fh, (WORD)chan, pData, (long)nMax, (TSTime)tFrom, eTime, &first, pFM);
    if (n > 0)
        tFirst = first;
    return S64Err(n);
}
