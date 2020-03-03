// s32priv.h
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

#ifndef __S32PRIV_H__
#define __S32PRIV_H__
//! \file s32priv.h
//! \brief Implementation header of the 64-bit interface to the 32-bit son file system

#include "s64.h"

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
    //! The object that implements a 32-bit SON data file using the 64-bit API.
    /*!
     The old file system uses the 32-bit ceds32::TSTime type for times rather than
     the #TSTime64 type used by the 64-bit library. It also has limits on the size
     of files that can be generated that are not present in the 64-bit library.

     This implementation makes all operations that have a chance of working with
     the old file format work correctly. Read operations work identically; writes
     are limited to the old time range (of course). More specialised functions
     like buffering are treated in as similar way as possible. The ChanUndelete()
     function is not supported and returns an error.

     The event and marker-based data types are different sizes in the 32-bit SON
     library. We expand the data sizes on read, and reduce them on write. On a read,
     all expanded space is zero filled.

     The aim is that users should not notice any difference between a Son32 file and
     a Son64 file as long as they do not exceed the limits of the 32-bit SON format.

     \warning If you are using this a template for interfacing you own file format
     to this API, be very cautious about declaring the entire class DllClass. This
     causes everything declared in the class to be exported. In the TSon32File case,
     all the private class members are simple types, so it saves a bit of typing to
     do this. However, in the TSon64File class, we declare all the exported member
     functions DllClass to avoid exporting huge quantities of unnecessay information
     about boost and std library objects.
    */
    class DllClass TSon32File : public ceds64::CSon64File
    {
    private:
        short m_fh;             //!< The handle to the file or -1 if no file
        double m_dBufferedSecs; //!< Buffering time in seconds
        int m_iCreateBig;       //!< -1, not specified, 0 create small, 1=create large
        size_t LimitWaveCount(TChanNum chan, TSTime64 tFrom, size_t count) const;

        // Items from here on are part of the defined interface CSon64File
    public:
        explicit TSon32File(int iBig = -1);
        virtual ~TSon32File();
#ifdef _UNICODE
        virtual int Create(const wchar_t* szName, uint16_t nChannels, uint32_t nFUser = 0);
        virtual int Open(const wchar_t* szName, int iOpenMode = 1, int flags = ceds64::eOF_none);
#endif
        virtual int Create(const char* szName, uint16_t nChannels, uint32_t nFUser = 0);
        virtual int Open(const char* szName, int iOpenMode = 1, int flags = ceds64::eOF_none);
        virtual bool CanWrite() const;
        virtual int Close();
        virtual int EmptyFile();
        virtual int GetFreeChan() const;
        virtual int Commit(int flags = 0);
        virtual bool IsModified() const;
        virtual int FlushSysBuffers();

        virtual double GetTimeBase() const;
        virtual void SetTimeBase(double dSecPerTick);
        virtual int SetExtraData(const void* pData, uint32_t nBytes, uint32_t nOffset);
        virtual int GetExtraData(void* pData, uint32_t nBytes, uint32_t nOffset);
        virtual uint32_t GetExtraDataSize() const;
        virtual int SetFileComment(int n, const char* szComment);
        virtual int GetFileComment(int n, int nSz = 0, char* szComment = nullptr) const;
        virtual int MaxChans() const;
        virtual int AppID(TCreator* pRead, const TCreator* pWrite = nullptr);
        virtual int TimeDate(TTimeDate* pTDGet, const TTimeDate* pTDSet = nullptr);
        virtual int GetVersion() const;
        virtual uint64_t FileSize() const;
        virtual uint64_t ChanBytes(TChanNum chan) const;
        virtual TSTime64 MaxTime(bool bReadChans = true) const;
        virtual void ExtendMaxTime(TSTime64 t);

        virtual TDataKind ChanKind(TChanNum chan) const;
        virtual TSTime64 ChanDivide(TChanNum chan) const;
        virtual double IdealRate(TChanNum chan, double dRate = -1.0);
        virtual int PhyChan(TChanNum) const;
        virtual int SetChanComment(TChanNum chan, const char* szComment);
        virtual int GetChanComment(TChanNum chan, int nSz = 0, char* szComment = nullptr) const;
        virtual int SetChanTitle(TChanNum chan, const char* szTitle);
        virtual int GetChanTitle(TChanNum chan, int nSz = 0, char* szTitle = nullptr) const;
        virtual int SetChanScale(TChanNum chan, double dScale);
        virtual int GetChanScale(TChanNum chan, double& dScale) const;
        virtual int SetChanOffset(TChanNum chan, double dOffset);
        virtual int GetChanOffset(TChanNum chan, double& dOffset) const;
        virtual int SetChanUnits(TChanNum chan, const char* szUnits);
        virtual int GetChanUnits(TChanNum chan, int nSz = 0, char* szUnits = nullptr) const;
        virtual TSTime64 ChanMaxTime(TChanNum chan) const;
        virtual TSTime64 PrevNTime(TChanNum chan, TSTime64 sTime, TSTime64 eTime = 0,
                                   uint32_t n = 1, const CSFilter* pFilter = nullptr, bool bAsWave = false); 
        virtual int ChanDelete(TChanNum chan);
        virtual int ChanUndelete(TChanNum chan, eCU action=eCU_kind);
        virtual int GetChanYRange(TChanNum chan, double& dLow, double& dHigh) const;
        virtual int SetChanYRange(TChanNum chan, double dLow, double dHigh);
        virtual int ItemSize(TChanNum chan) const;

        virtual void Save(int chan, TSTime64 t, bool bSave);
        virtual void SaveRange(int chan, TSTime64 tFrom, TSTime64 tUpto);
        virtual bool IsSaving(TChanNum chan, TSTime64 tAt) const;
        virtual int NoSaveList(TChanNum chan, TSTime64* pTimes, int nMax, TSTime64 tFrom = -1, TSTime64 tUpto = TSTIME64_MAX) const;
        virtual int LatestTime(int chan, TSTime64 t);
        virtual double SetBuffering(int chan, size_t nBytes, double dSeconds = 0.0);

        virtual int SetEventChan(TChanNum chan, double dRate, TDataKind evtKind = EventFall, int iPhyCh=-1);
        virtual int WriteEvents(TChanNum chan, const TSTime64* pData, size_t count);
        virtual int ReadEvents(TChanNum chan, TSTime64* pData, int nMax, TSTime64 tFrom, TSTime64 tUpto, const CSFilter* pFilter = nullptr);

        virtual int SetMarkerChan(TChanNum chan, double dRate, TDataKind kind = Marker, int iPhyChan = -1);
        virtual int WriteMarkers(TChanNum chan, const TMarker* pData, size_t count);
        virtual int ReadMarkers(TChanNum chan, TMarker* pData, int nMax, TSTime64 tFrom, TSTime64 tUpto, const CSFilter* pFilter = nullptr);
        virtual int EditMarker(TChanNum chan, TSTime64 t, const TMarker* pM, size_t nCopy = sizeof(TMarker));

        virtual int SetLevelChan(TChanNum chan, double dRate, int iPhyChan = -1);
        virtual int SetInitLevel(TChanNum chan, bool bLevel);
        virtual int WriteLevels(TChanNum chan, const TSTime64* pData, size_t count);
        virtual int ReadLevels(TChanNum chan, TSTime64* pData, int nMax, TSTime64 tFrom, TSTime64 tUpto, bool& bLevel);

        virtual int SetTextMarkChan(TChanNum chan, double dRate, size_t nMax, int iPhyChan = -1);
        virtual int SetExtMarkChan(TChanNum chan, double dRate, TDataKind kind, size_t nRows, size_t nCols = 1, int iPhyChan = -1, TSTime64 lDvd = 0, int nPre=0);
        virtual int GetExtMarkInfo(TChanNum chan, size_t *pRows = nullptr, size_t* pCols = nullptr) const;

        virtual int SetWaveChan(TChanNum chan, TSTime64 lDvd, TDataKind wKind, double dRate = 0.0, int iPhyCh=-1);
        virtual TSTime64 WriteWave(TChanNum chan, const short* pData, size_t count, TSTime64 tFrom);
        virtual TSTime64 WriteWave(TChanNum chan, const float* pData, size_t count, TSTime64 tFrom);
        virtual int ReadWave(TChanNum chan, short* pData, int nMax, TSTime64 tFrom, TSTime64 tUpto, TSTime64& tFirst, const CSFilter* pFilter = nullptr);
        virtual int ReadWave(TChanNum chan, float* pData, int nMax, TSTime64 tFrom, TSTime64 tUpto, TSTime64& tFirst, const CSFilter* pFilter = nullptr);

        virtual int WriteExtMarks(TChanNum chan, const TExtMark* pData, size_t count);
        virtual int ReadExtMarks(TChanNum chan, TExtMark* pData, int nMax, TSTime64 tFrom, TSTime64 tUpto, const CSFilter* pFilter = nullptr);
    
        // This is the end of the defined interface
    };
};
#undef DllClass
#endif