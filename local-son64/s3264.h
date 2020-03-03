// s3264.h
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
#ifndef __S3264_H__
#define __S3264_H__
//! \file s3264.h
//! \brief Code to make a son64 file look like a son32 file
/*! \internal
Do not put sonintl.h into a namespace as we link to the non-namespaced son32.dll.
This makes for a bit of confusion as the implementation uses namespace ceds64, so
types such as TDataKind need clarification: ::TDataKind (for the Son32 version)
or ceds64::TDataKind for the Son64 version. In this header, TDataKind is the Son32
version, but in the implementation, it would be the Son64 version. The two types
are, fortunately, identical.
*/
#include "sonintl.h"

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

#ifdef __cplusplus
extern "C" {
#endif

typedef void* TpS64;        //!< the opaque pointer to our CSon64File

/*
** Now definitions of the functions defined in the code
*/
DllClass SONAPI(TpS64) S32OpenOldFile(TpCFName name, int iOpenMode);
DllClass SONAPI(TpS64) S32OpenNewFile(TpCFName name, short fMode, WORD extra);

DllClass SONAPI(BOOLEAN) S32CanWrite(TpS64 fh);
DllClass SONAPI(short) S32CloseFile(TpS64 fh);
DllClass SONAPI(short) S32EmptyFile(TpS64 fh);
DllClass SONAPI(short) S32SetBuffSpace(TpS64 fh);
DllClass SONAPI(short) S32GetFreeChan(TpS64 fh);
DllClass SONAPI(void) S32SetFileClock(TpS64 fh, WORD usPerTime, WORD timePerADC);
DllClass SONAPI(short) S32SetADCChan(TpS64 fh, WORD chan, short sPhyCh, short dvd,
                 int lBufSz, TpCStr szCom, TpCStr szTitle, float fRate,
                 float scl, float offs, TpCStr szUnt);
DllClass SONAPI(short) S32SetADCMarkChan(TpS64 fh, WORD chan, short sPhyCh, short dvd,
                 int lBufSz, TpCStr szCom, TpCStr szTitle, float fRate, float scl,
                 float offs, TpCStr szUnt, WORD points, short preTrig);
DllClass SONAPI(short) S32SetWaveChan(TpS64 fh, WORD chan, short sPhyCh, TSTime dvd,
                 int lBufSz, TpCStr szCom, TpCStr szTitle,
                 float scl, float offs, TpCStr szUnt);
DllClass SONAPI(short) S32SetWaveMarkChan(TpS64 fh, WORD chan, short sPhyCh, TSTime dvd,
                 int lBufSz, TpCStr szCom, TpCStr szTitle, float fRate, float scl,
                 float offs, TpCStr szUnt, WORD points, short preTrig, int nTrace);
DllClass SONAPI(short) S32SetRealMarkChan(TpS64 fh, WORD chan, short sPhyCh,
                 int lBufSz, TpCStr szCom, TpCStr szTitle, float fRate,
                 float min, float max, TpCStr szUnt, WORD points);
DllClass SONAPI(short) S32SetTextMarkChan(TpS64 fh, WORD chan, short sPhyCh,
                 int lBufSz, TpCStr szCom, TpCStr szTitle,
                 float fRate, TpCStr szUnt, WORD points);
DllClass SONAPI(void) S32SetInitLow(TpS64 fh, WORD chan, BOOLEAN bLow);
DllClass SONAPI(short) S32SetEventChan(TpS64 fh, WORD chan, short sPhyCh,
                 int lBufSz, TpCStr szCom, TpCStr szTitle, float fRate, TDataKind evtKind);

DllClass SONAPI(short) S32SetBuffering(TpS64 fh, int nChan, int nBytes);
DllClass SONAPI(short) S32UpdateStart(TpS64 fh);
DllClass SONAPI(void) S32SetFileComment(TpS64 fh, WORD which, TpCStr szFCom);
DllClass SONAPI(void) S32GetFileComment(TpS64 fh, WORD which, TpStr pcFCom, short sMax);
DllClass SONAPI(void) S32SetChanComment(TpS64 fh, WORD chan, TpCStr szCom);
DllClass SONAPI(void) S32GetChanComment(TpS64 fh, WORD chan, TpStr pcCom, short sMax);
DllClass SONAPI(void) S32SetChanTitle(TpS64 fh, WORD chan, TpCStr szTitle);
DllClass SONAPI(void) S32GetChanTitle(TpS64 fh, WORD chan, TpStr pcTitle);
DllClass SONAPI(void) S32GetIdealLimits(TpS64 fh, WORD chan, TpFloat pfRate, TpFloat pfMin, TpFloat pfMax);
DllClass SONAPI(WORD) S32GetusPerTime(TpS64 fh);
DllClass SONAPI(WORD) S32GetTimePerADC(TpS64 fh);
DllClass SONAPI(void) S32SetADCUnits(TpS64 fh, WORD chan, TpCStr szUnt);
DllClass SONAPI(void) S32SetADCOffset(TpS64 fh, WORD chan, float offset);
DllClass SONAPI(void) S32SetADCScale(TpS64 fh, WORD chan, float scale);
DllClass SONAPI(void) S32GetADCInfo(TpS64 fh, WORD chan, TpFloat scale, TpFloat offset,
                 TpStr pcUnt, TpWORD points, TpShort preTrig);
DllClass SONAPI(void) S32GetExtMarkInfo(TpS64 fh, WORD chan, TpStr pcUnt,
                 TpWORD points, TpShort preTrig);

DllClass SONAPI(short) S32WriteEventBlock(TpS64 fh, WORD chan, TpSTime plBuf, int count);
DllClass SONAPI(short) S32WriteMarkBlock(TpS64 fh, WORD chan, TpMarker pM, int count);
DllClass SONAPI(TSTime) S32WriteADCBlock(TpS64 fh, WORD chan, TpAdc psBuf, int count, TSTime sTime);
DllClass SONAPI(short) S32WriteExtMarkBlock(TpS64 fh, WORD chan, TpMarker pM, int count);

DllClass SONAPI(short) S32Save(TpS64 fh, int nChan, TSTime sTime, BOOLEAN bKeep);
DllClass SONAPI(short) S32KeepRange(TpS64 fh, int nChan, TSTime sTime, TSTime eTime, BOOLEAN bKeep);
DllClass SONAPI(short) S32SaveRange(TpS64 fh, int nChan, TSTime sTime, TSTime eTime);
DllClass SONAPI(short) S32KillRange(TpS64 fh, int nChan, TSTime sTime, TSTime eTime);
DllClass SONAPI(short) S32NoSaveList(TpS64 fh, WORD wChan, TSTime* pTimes);
DllClass SONAPI(short) S32IsSaving(TpS64 fh, int nChan);
DllClass SONAPI(DWORD) S32FileBytes(TpS64 fh);
DllClass SONAPI(DWORD) S32ChanBytes(TpS64 fh, WORD chan);

DllClass SONAPI(short) S32LatestTime(TpS64 fh, WORD chan, TSTime sTime);
DllClass SONAPI(short) S32CommitIdle(TpS64 fh);
DllClass SONAPI(short) S32CommitFile(TpS64 fh, BOOLEAN bDelete);
DllClass SONAPI(short) S32CommitFileEx(TpS64 fh, int flags);

DllClass SONAPI(int) S32GetEventData(TpS64 fh, WORD chan, TpSTime plTimes, int max,
                  TSTime sTime, TSTime eTime, TpBOOL levLowP, 
                  TpFilterMask pFiltMask);
DllClass SONAPI(int) S32GetMarkData(TpS64 fh, WORD chan,TpMarker pMark, int max,
                  TSTime sTime,TSTime eTime, TpFilterMask pFiltMask);
DllClass SONAPI(int) S32GetADCData(TpS64 fh,WORD chan,TpAdc adcDataP, int max,
                  TSTime sTime,TSTime eTime,TpSTime pbTime, 
                  int nTr, TpFilterMask pFiltMask);

DllClass SONAPI(int) S32GetExtMarkData(TpS64 fh, WORD chan, TpMarker pMark, int max,
                  TSTime sTime,TSTime eTime, TpFilterMask pFiltMask);
DllClass SONAPI(int) S32GetExtraDataSize(TpS64 fh);
DllClass SONAPI(int) S32GetVersion(TpS64 fh);
DllClass SONAPI(short) S32GetExtraData(TpS64 fh, TpVoid buff, WORD bytes,
                  WORD offset, BOOLEAN writeIt);
DllClass SONAPI(short) S32SetMarker(TpS64 fh, WORD chan, TSTime time, TpMarker pMark,
                  WORD size);
DllClass SONAPI(short)  S32ChanDelete(TpS64 fh, WORD chan);
DllClass SONAPI(TDataKind) S32ChanKind(TpS64 fh, WORD chan);
DllClass SONAPI(TSTime) S32ChanDivide(TpS64 fh, WORD chan);
DllClass SONAPI(WORD)   S32ItemSize(TpS64 fh, WORD chan);
DllClass SONAPI(TSTime) S32ChanMaxTime(TpS64 fh, WORD chan);
DllClass SONAPI(TSTime) S32MaxTime(TpS64 fh);

DllClass SONAPI(TSTime) S32LastTime(TpS64 fh, WORD wChan, TSTime sTime, TSTime eTime,
                    TpVoid pvVal, TpMarkBytes pMB,
                    TpBOOL pbMk, TpFilterMask pFiltMask);

DllClass SONAPI(TSTime) S32LastPointsTime(TpS64 fh, WORD wChan, TSTime sTime, TSTime eTime,
                    int lPoints, BOOLEAN bAdc, TpFilterMask pFiltMask);

DllClass SONAPI(int) S32FileSize(TpS64 fh);

// The following do not require access to the file, so should be handled by the
// SONxxx routines
//SONAPI(int) S32MarkerItem(TpS64 fh, WORD wChan, TpMarker pBuff, int n,
//                                          TpMarker pM, TpVoid pvData, BOOLEAN bSet);
//SONAPI(int) S32Filter(TpMarker pM, TpFilterMask pFM);
//SONAPI(int) S32FControl(TpFilterMask pFM, int layer, int item, int set);
//SONAPI(BOOLEAN) S32FEqual(TpFilterMask pFiltMask1, TpFilterMask pFiltMask2);
//SONAPI(int) S32FActive(TpFilterMask pFM);   // added 14/May/02
//SONAPI(int) S32FMode(TpFilterMask pFM, int lNew);

/****************************************************************************
** New things added at Revision 6
*/
DllClass SONAPI(TpS64) S32CreateFile(TpCFName name, int nChannels, WORD extra);
DllClass SONAPI(int) S32MaxChans(TpS64 fh);
DllClass SONAPI(int) S32PhyChan(TpS64 fh, WORD wChan);
DllClass SONAPI(float) S32IdealRate(TpS64 fh, WORD wChan, float fIR);
DllClass SONAPI(void) S32YRange(TpS64 fh, WORD chan, TpFloat pfMin, TpFloat pfMax);
DllClass SONAPI(int) S32YRangeSet(TpS64 fh, WORD chan, float fMin, float fMax);
DllClass SONAPI(int) S32MaxItems(TpS64 fh, WORD chan);
DllClass SONAPI(int) S32PhySz(TpS64 fh, WORD chan);
DllClass SONAPI(int) S32Blocks(TpS64 fh, WORD chan);
DllClass SONAPI(int) S32DelBlocks(TpS64 fh, WORD chan);
DllClass SONAPI(int) S32SetRealChan(TpS64 fh, WORD chan, short sPhyChan, TSTime dvd,
                 int lBufSz, TpCStr szCom, TpCStr szTitle,
                 float scale, float offset, TpCStr szUnt);
DllClass SONAPI(TSTime) S32WriteRealBlock(TpS64 fh, WORD chan, TpFloat pfBuff, int count, TSTime sTime);
DllClass SONAPI(int) S32GetRealData(TpS64 fh, WORD chan, TpFloat pfData, int max,
                  TSTime sTime,TSTime eTime,TpSTime pbTime, int nTr, TpFilterMask pFiltMask);
DllClass SONAPI(int) S32TimeDate(TpS64 fh, TSONTimeDate* pTDGet, const TSONTimeDate* pTDSet);
DllClass SONAPI(double) S32TimeBase(TpS64 fh, double dTB);
DllClass SONAPI(int) S32AppID(TpS64 fh, TSONCreator* pCGet, const TSONCreator* pCSet);
DllClass SONAPI(int) S32ChanInterleave(TpS64 fh, WORD chan);

/* Version 7 */
DllClass SONAPI(int) S32ExtMarkAlign(TpS64 fh, int n);

/* Version 9 */
DllClass SONAPI(double) S32FileSizeD(TpS64 fh);
DllClass SONAPI(double) S32ChanBytesD(TpS64 fh, WORD chan);
DllClass SONAPI(TpS64) S32CreateFileEx(TpCFName name, int nChannels, WORD extra, int iBigFile);
DllClass SONAPI(int) S32IsBigFile(TpS64 fh);
DllClass SONAPI(int) S32UpdateHeader(TpS64 fh);
DllClass SONAPI(int) S32UpdateMaxTimes(TpS64 fh);
DllClass SONAPI(void) S32ExtendMaxTime(TpS64 fh, TSTime time);
DllClass SONAPI(void) S32SetPhySz(TpS64 fh, WORD wChan, int lSize);
#ifdef __cplusplus
}
#endif

#undef DllClass
#endif
