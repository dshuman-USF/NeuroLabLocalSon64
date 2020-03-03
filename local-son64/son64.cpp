// Implementation file for the 64-bit version of the SON library
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

//! \file
//! \brief The exposed file interface of the SON64 library
#include <assert.h>
#include <iostream>
#include "s64priv.h"
#include "s64chan.h"
#include "s64range.h"

using namespace ceds64;
//-----------------TSon64File -----------------------------------------------

TSon64File::TSon64File()
    : m_file( NOFILE_ID )
    , m_bReadOnly( false )
    , m_bHeadDirty( false )
    , m_bOldFile( false )
    , m_dBufferedSecs( 0.0 )
{
    m_Head.Init(32, 0);         // make it tidy
}

//! Son64 file destructor
/*!
If there is an open data file associated with the TSon64File object, the Close()
function is called to tidy it up, then all resources associated with the file are
released.
*/
TSon64File::~TSon64File()
{
    if (m_file != NOFILE_ID)
        Close();
}

// _UNICODE is ONLY defined in Windows. In Linux we only deal with UTF-8
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
        vector<wchar_t> vw(nOut);
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

int TSon64File::Create(const wchar_t* szName, uint16_t nChans, uint32_t nFUser)
{
    if (nChans < MINCHANS)       // some basic sanity checks
        nChans = MINCHANS;
    else if ((nChans > MAXCHANS) || (nFUser > MAXHEADUSER))
        return BAD_PARAM;

    THeadLock lock(m_mutHead);      // acquire head lock
    if (m_file != NOFILE_ID)        // must not be open already
        return NO_ACCESS;

#if   S64_OS == S64_OS_WINDOWS
    m_file = CreateFile(szName, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
#ifdef SON_WRITETHROUGH
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
#else
        FILE_ATTRIBUTE_NORMAL,
#endif
        NULL);
    if (m_file == INVALID_HANDLE_VALUE)
#elif S64_OS == S64_OS_LINUX
    m_file = open64(szName, O_RDWR | O_CREAT | O_TRUNC | O_BINARY, S_IREAD | S_IWRITE);
    if (m_file < 0)
#endif
        return NO_FILE;

    m_bReadOnly = false;            // must be able to write!
    m_Head.Init(nChans, nFUser);    // create the header
    int err = WriteHeader(&m_Head, sizeof(m_Head), 0);
    if (err == 0)
        err = ZeroExtraData();      // make sure the extra data all holds 0's
    if (err == 0)
    {
        m_vChanHead.resize(nChans);  // allocate space for the channel head
        err = WriteHeader(&m_vChanHead[0], sizeof(TChanHead)*nChans, m_Head.m_nChanStart);
        m_vChan.resize(nChans);      // allocate pointer space for the channels
        fill_n(m_vChan.begin(), nChans, nullptr);
    }

    if (err == 0)
    {
        m_bHeadDirty = false;
        err = WriteStringStore();   // so the file is in a decent state
    }

    if (err)                        // if we have a problem we must close the file
        Close();                    // bail out

    return err;
}

int TSon64File::Open(const wchar_t* szName, int iOpenMode, int flags)
{
    THeadLock lock(m_mutHead);      // acquire the head lock
    if (m_file != NOFILE_ID)        // must not be open already
        return NO_ACCESS;

    while (true)
    {
        m_bReadOnly = iOpenMode > 0;   // read only
#if   S64_OS == S64_OS_WINDOWS
        const DWORD dwAccess = m_bReadOnly ? GENERIC_READ : GENERIC_READ | GENERIC_WRITE;
        m_file = CreateFile(szName, dwAccess, FILE_SHARE_READ, NULL, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, NULL);
#elif S64_OS == S64_OS_LINUX
        m_file = open64(szName, m_bReadOnly ? O_BINARY | O_RDONLY : O_BINARY | O_RDWR);
#endif
        if (m_file == NOFILE_ID)    // if the file could not be opened..
        {
            if (iOpenMode < 0)
                iOpenMode = 1;      // ...try again in read only mode
            else                    // if we still can;t open the file
                return NO_FILE;     // bail out and return an error
        }
        else
            break;
    }

    // We now have the underlying file open. Read in the header and see if it might be
    // one of our files.
    int err = ReadHeader(&m_Head, sizeof(TFileHead), 0);  // read the head
    if (err == 0)
        err = m_Head.Verify();      // check that this looks like a file header

    if (err == 0)
        m_vChanHead.resize(m_Head.m_nChannels); // space for the channel headers

    // If head is OK, read the string store
    if (err == 0)
    {
        err = ReadStringStore();
        if (err && flags & eOF_test)    // attempt to keep going if in test mode
            err = 0;
    }

    // If string store is OK, read in the channels, then create them all
    if (err == 0)
    {
        err = ReadHeader(&m_vChanHead[0], m_Head.m_nChannels*sizeof(TChanHead), m_Head.m_nChanStart);
        if (err == 0)
            err = CreateChannelsFromHeaders();
    }

    // If the file on disk is bigger than the next block we would allocate for it, we have a
    // problem, as the file was probably not closed properly. Any writes to it will cause it
    // to overwrite possible wanted data.
    if (err == 0)
    {
        TDiskOff nFileSizeOnDisk = GetFileSize();   // physical file size on disk
        if (nFileSizeOnDisk - m_Head.m_doNextBlock > DBSize - DLSize)
            err = MORE_DATA;
    }

    // If there was a problem we must kill off the file handle unless testing
    if (err && !(flags & eOF_test))     // If test flag is set we open regardless
    {
#if   S64_OS == S64_OS_WINDOWS
        CloseHandle(m_file);
#elif S64_OS == S64_OS_LINUX
        close(m_file);
#endif
        m_file = NOFILE_ID;
    }

    m_bOldFile = true;          // signal this is an old file
    return err;
}

#endif
//-----------------------------------------------------------------------------------------

// Create a new, empty file on disk. If SON_WRITETHROUGH flag is set, we open the file
int TSon64File::Create(const char* szName, uint16_t nChans, uint32_t nFUser)
{
#ifdef _UNICODE
    return Create(s2ws(szName).c_str(), nChans, nFUser);
#else
    if (nChans < MINCHANS)       // some basic sanity checks
        nChans = MINCHANS;
    else if ((nChans > MAXCHANS) || (nFUser > MAXHEADUSER))
        return BAD_PARAM;

    THeadLock lock(m_mutHead);      // acquire head lock
    if (m_file != NOFILE_ID)        // must not be open already
        return NO_ACCESS;

#if   S64_OS == S64_OS_WINDOWS
    m_file = CreateFile(szName, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
#ifdef SON_WRITETHROUGH
                       FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
#else
                       FILE_ATTRIBUTE_NORMAL,
#endif
                       NULL);
    if (m_file == INVALID_HANDLE_VALUE)
#elif S64_OS == S64_OS_LINUX
    m_file = open64(szName, O_RDWR | O_CREAT | O_TRUNC | O_BINARY, S_IREAD | S_IWRITE);
    if (m_file < 0 )
#endif
        return NO_FILE;

    m_bReadOnly = false;            // must be able to write!
    m_Head.Init(nChans, nFUser);    // create the header
    int err = WriteHeader(&m_Head, sizeof(m_Head), 0);
    if (err == 0)
        err = ZeroExtraData();      // make sure the extra data all holds 0's
    if (err == 0)
    {
        m_vChanHead.resize(nChans);  // allocate space for the channel head
        err = WriteHeader(&m_vChanHead[0], sizeof(TChanHead)*nChans, m_Head.m_nChanStart);
        m_vChan.resize(nChans);      // allocate pointer space for the channels
        fill_n(m_vChan.begin(), nChans, nullptr);
    }

    if (err == 0)
    {
        m_bHeadDirty = false;
        err = WriteStringStore();   // so the file is in a decent state
    }

    if (err)                        // if we have a problem we must close the file
        Close();                    // bail out

    return err;
#endif
}

//-----------------------------------------------------------------------------------------
// Strings cause linking problems with DLL. This is kept for compatibility reasons.

// This is the preferred entry point
int TSon64File::Open(const char* szName, int iOpenMode, int flags)
{
#ifdef _UNICODE
    return Open(s2ws(szName).c_str(), iOpenMode, flags);
#else
    THeadLock lock(m_mutHead);      // acquire the head lock
    if (m_file != NOFILE_ID)        // must not be open already
       return NO_ACCESS;

    while (true)
    {
        m_bReadOnly = iOpenMode > 0;   // read only
#if   S64_OS == S64_OS_WINDOWS
        const DWORD dwAccess = m_bReadOnly ? GENERIC_READ : GENERIC_READ | GENERIC_WRITE;
        m_file = CreateFile(szName, dwAccess, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                      FILE_ATTRIBUTE_NORMAL, NULL);
#elif S64_OS == S64_OS_LINUX
        m_file = open64(szName, m_bReadOnly ? O_BINARY|O_RDONLY : O_BINARY|O_RDWR);
#endif
        if (m_file == NOFILE_ID)    // if the file could not be opened..
        {
            if (iOpenMode < 0)
                iOpenMode = 1;      // ...try again in read only mode
            else                    // if we still can;t open the file
                return NO_FILE;     // bail out and return an error
        }
        else
            break;
    }

    // We now have the underlying file open. Read in the header and see if it might be
    // one of our files.
    int err = ReadHeader(&m_Head, sizeof(TFileHead), 0);  // read the head
    if (err == 0)
        err = m_Head.Verify();      // check that this looks like a file header

    if (err == 0)
        m_vChanHead.resize(m_Head.m_nChannels); // space for the channel headers

    // If head is OK, read the string store
    if (err == 0)
        err = ReadStringStore();

    // If string store is OK, read in the channels, then create them all
    if (err == 0)
    {
        err = ReadHeader(&m_vChanHead[0], m_Head.m_nChannels*sizeof(TChanHead), m_Head.m_nChanStart);
        if (err == 0)
            err = CreateChannelsFromHeaders();
    }

    // If the file on disk is bigger than the next block we would allocate for it, we have a
    // problem, as the file was probably not closed properly. Any writes to it will cause it
    // to overwrite possible wanted data.
    if (err == 0)
    {
        TDiskOff nFileSizeOnDisk = GetFileSize();   // physical file size on disk
        if (nFileSizeOnDisk - m_Head.m_doNextBlock > DBSize - DLSize)
            err = MORE_DATA;
    }

    // If there was a problem we must kill off the file handle unless testing
    if (err && !(flags & eOF_test))     // If test flag is set we open regardless
    {
#if   S64_OS == S64_OS_WINDOWS
        CloseHandle(m_file);
#elif S64_OS == S64_OS_LINUX
        close(m_file);
#endif
        m_file = NOFILE_ID;
    }

    m_bOldFile = true;          // signal this is an old file
    return err;
#endif
}

// Set the time base for the file. If we make a change, mark the head as needing
// writing. However, if m_bReadOnly it will not be written!
void TSon64File::SetTimeBase(double dSecPerTick)
{
    THeadLock lock(m_mutHead);
    if ((dSecPerTick > 0.0) && (dSecPerTick != m_Head.m_dSecPerTick))
    {
        m_Head.m_dSecPerTick = dSecPerTick;
        m_bHeadDirty = true;
    }
}

double TSon64File::GetTimeBase() const
{
    THeadLock lock(m_mutHead);
    return m_Head.m_dSecPerTick;
}

// Report if any part of the file is dirty, so needs a commit.
bool TSon64File::IsModified() const
{
    {
        THeadLock lock(m_mutHead);
        if (m_bHeadDirty)
            return true;
        if (m_ss.IsModified())
            return true;
    }

    TChRdLock lock(m_mutChans);     // Read lock we are not changing the #chans
    for (const auto& pChan : m_vChan)
    {
        if (pChan && pChan->IsModified())
            return true;
    }

    return false;
}

// Write any parts of the file that need writing. Note that this just means that the
// data has made it as far as the operating system buffers. To flush everything to the
// disk, call FlushSysBuffers() which may or may not do this, but can be inefficient as
// it may empty the disk cache.
int TSon64File::Commit(int flags)
{
    if (m_bReadOnly)
        return READ_ONLY;

    int err = 0;

    // Commit any outstanding channel writes (may occasionally lock the head)
    if ((flags & eCF_headerOnly) == 0)
    {
        TChRdLock lock(m_mutChans);         // read lock as not changing the #chans
        for (auto& pChan : m_vChan)
        {
            if (pChan)
            {
                int locErr = pChan->Commit();
                if (locErr && (err == 0))   // save the first error
                    err = locErr;
            }
        }
    }

    {
        THeadLock lock(m_mutHead);

        // We must write the string store first as it may increase the header extensions
        if (m_ss.IsModified())
        {
            int locErr = WriteStringStore();
            if (locErr && (err == 0))
                err = locErr;
        }

        if (m_bHeadDirty)
        {
            int locErr = WriteHeader(&m_Head, sizeof(m_Head), 0);
            m_bHeadDirty = locErr != 0;
            if (locErr && (err == 0))
                err = locErr;
        }
    }

    if (flags & eCF_delBuffer)
        SetBuffering(-1, 0, 0);

    // This next is very expensive as it flushes all cached and dirty data to the disk. With
    // a big disk with hardware buffering, this can be very slow
    if (flags & eCF_flushSys)
    {
        int locErr = FlushSysBuffers();
        if (locErr && (err == 0))
            err = locErr;
    }

    return err;
}

// Where supported, this tells the OS to make sure that data written to OS buffers ends
// up on disk. This can be very inefficient as it is allowed to remove all data from disk
// caches, resulting in very slow operation until data is reloaded.
int TSon64File::FlushSysBuffers()
{
    if (m_bReadOnly)
        return READ_ONLY;

    TFileLock lock(m_mutFile);      // move outside #if when linux flush done
#if   S64_OS == S64_OS_WINDOWS
    FlushFileBuffers(m_file);
#elif S64_OS == S64_OS_LINUX
    fsync(m_file);                  // flush buffers to the file
#endif
    return S64_OK;
}

// Close down an open file. This should write any dirty sections in the file header and
// channel information. For now, just close the handle. We assume that opening and closing
// of files is carefully controlled. We do not expect competing threads to close down the
// same file.
int TSon64File::Close()
{
    if (m_file == NOFILE_ID)
        return NO_FILE;

    int err = m_bReadOnly ? S64_OK : Commit();
    FlushSysBuffers();          // Does nothing if m_bReadOnly is true

    TFileLock lock(m_mutFile);
#if   S64_OS == S64_OS_WINDOWS
    CloseHandle(m_file);
#elif S64_OS == S64_OS_LINUX
    close(m_file);
#endif
    m_file = NOFILE_ID;
    return err;
}

//! Keep everything the same, but set all pointers back to the start of the channels
/*!
The idea is that we want to quickly restart, keeping everything the same.
\return     S64_OK (0) if done, else an error code.
*/
int TSon64File::EmptyFile()
{
    if (m_file == NOFILE_ID)
        return NO_FILE;

    if (m_bReadOnly)
        return READ_ONLY;

    int err = 0;

    TChRdLock lock(m_mutChans);     // we are not changing the #chans
    for (auto& pChan : m_vChan)
    {
        if (pChan)                  // beware, the pointer may not be set                     
        {
            int iErr = pChan->EmptyForReuse();
            if (err == 0)           // report the first error found
                err = iErr;
        }
    }
    ExtendMaxTime(-1);              // say the file has no data anymore

    return err;
}

//! Generic read from the file
/*!
\internal
This is exposed through the DLL interface so that external code can read the file for
diagnostic purposes (for example, the S64Fix program does this).
\param pBuffer  Target of the read.
\param bytes    The number of bytes to read. This must be matched exactly, else an error.
\param offset   The file offset where the read shall take place.
\return         S64_OK (0) for success or a negative code
*/
int TSon64File::Read(void* pBuffer, uint32_t bytes, TDiskOff offset)
{
    if (offset < 0)
        return PAST_SOF;

    int err = S64_OK;

    TFileLock lock(m_mutFile);  // acquire file lock

    assert(m_file != NOFILE_ID);
    if (m_file == NOFILE_ID)
        return NO_FILE;

#if S64_OS == S64_OS_WINDOWS
    LARGE_INTEGER llOffset;
    llOffset.QuadPart = (LONGLONG)offset;
    if (SetFilePointerEx(m_file, llOffset, NULL, FILE_BEGIN) == 0)
        err = BAD_READ;
    else
    {
        DWORD dwRead;
        if (!ReadFile(m_file, pBuffer, bytes, &dwRead, NULL))
        {
            DWORD dwError = GetLastError();     // attempt to find out why
            int iRetry = 100;                   // a retry count

            // If we fail due to network problem, then try very hard to
            // recover as it may go away with a few retries...
            err = BAD_READ;
            while (((dwError == ERROR_NETNAME_DELETED) ||
                    (dwError == ERROR_NETWORK_BUSY)) &&
                   (--iRetry > 0))
            {
                if (ReadFile(m_file, pBuffer, bytes, &dwRead, NULL))
                {
                    err = 0;
#ifdef _DEBUG
//                    OutputDebugString("TSon64File::Read() Recovered network failure\n");
#endif
                    break;
                }
                dwError = GetLastError();       // attempt to find out why
            }
        }

        if ((err == 0) && (dwRead != bytes))
            err = BAD_READ;
    }
#elif S64_OS == S64_OS_LINUX
    if (lseek64(m_file, offset, SEEK_SET) != offset)
        return BAD_READ;
    if (read(m_file, pBuffer, bytes) != bytes)
        return BAD_READ;
#endif  // OS dependant code

    return err;
}

//! Get the physical size of the data file on disk by asking the OS for it
TDiskOff TSon64File::GetFileSize()
{
    TFileLock lock(m_mutFile);  // acquire file lock

    assert(m_file != NOFILE_ID);
    if (m_file == NOFILE_ID)
        return NO_FILE;

#if S64_OS == S64_OS_WINDOWS
    LARGE_INTEGER llOffset;
    if (GetFileSizeEx(m_file, &llOffset))
        return (TDiskOff)llOffset.QuadPart;
    else
        return NO_ACCESS;
#elif S64_OS == S64_OS_LINUX
    TDiskOff off = (TDiskOff)lseek64(m_file, 0, SEEK_END);
    return  off >= 0 ? off : NO_ACCESS;
#endif  // OS dependant code

}

//! Generic write to the file
/*!
\internal
\param pBuffer  memory to be written
\param bytes    Bytes to be written
\param offset   Where we are to write
\return         S64_OK (0) or an error code
*/
int TSon64File::Write(const void* pBuffer, uint32_t bytes, TDiskOff offset)
{
    int err = S64_OK;
    assert(m_file != NULL);
    
    if (m_file == NOFILE_ID)
        return NO_FILE;

    if (m_bReadOnly)
        return READ_ONLY;

    if (offset < 0)
        return PAST_SOF;

    TFileLock lock(m_mutFile);  // acquire file lock
#if S64_OS == S64_OS_WINDOWS
    DWORD   dwWritten;
    LARGE_INTEGER llOffset;
    llOffset.QuadPart = (LONGLONG)offset;
    if (SetFilePointerEx(m_file, llOffset, NULL, FILE_BEGIN) == 0)
        err = BAD_WRITE;
    else if (!WriteFile(m_file, pBuffer, bytes, &dwWritten, NULL))
        err = BAD_WRITE;
    else if (dwWritten != bytes)
        err = BAD_WRITE;
#elif S64_OS == S64_OS_LINUX
    if (lseek64(m_file, offset, SEEK_SET) != offset)
        return BAD_WRITE;
    if (write(m_file, pBuffer, bytes) != bytes)
        return BAD_WRITE;
#endif

    return err;
}

//====================== Get and Set File and channel comments =============
int TSon64File::SetFileComment(int n, const char* szComment)
{
    if ((n < 0) || (n>=FHComments))
        return BAD_PARAM;

    THeadLock lock(m_mutHead);      // protect the head and string table
    m_Head.m_comments[n] = m_ss.Add(szComment, m_Head.m_comments[n]);
    m_bHeadDirty = true;            // needs writing to disk
    return S64_OK;
}

//! Internal code to copy std::string to char* limiting size
/*!
\internal
\param sz   The target space. If nullptr no copy is done.
\param nSz  The size of the target space. If <=0 no copy is done. Returned
            strings are 0-terminated. We do not return partial UTF-8 characters.
\param str  The string to copy.
\return     The length of sz needed to collect the entire string, including the
            trailing 0. If nSz is less, the string will be truncated.
*/
static int String2SZ(char* sz, int nSz, const string& str)
{
    int n = static_cast<int>(str.size())+1;   // Space needed
    if (sz && (nSz > 0))
    {
        if (n <= nSz)                       // if copy all
            memcpy(sz, str.c_str(), n);     // just do it
        else
        {
            if ((str[nSz - 1] & 0xc0) == 0x80) // If first deleted is a trail code
            {
                // We must delete the entire UTF8 character. Remove trail codes
                --nSz;
                while ((nSz > 0) && ((str[nSz - 1] & 0xc0) == 0x80))
                    --nSz;

                // Now remove the lead byte (which should be present)
                if ((nSz > 0) && ((str[nSz - 1] & 0xc0) == 0xc0))
                    --nSz;
            }

            if (nSz-1 > 0)
                memcpy(sz, str.c_str(), nSz-1); // copy the data
            sz[nSz-1] = 0;                      // make sure terminated
        }
    }
    return n;
}

int TSon64File::GetFileComment(int n, int nSz, char* szComment) const
{
    if ((n < 0) || (n >= FHComments))
        return BAD_PARAM;

    THeadLock lock(m_mutHead);      // protect the head and string table
    const auto& comment = m_ss.String(m_Head.m_comments[n]);
    return String2SZ(szComment, nSz, comment);
}

int TSon64File::AppID(TCreator* pRead, const TCreator* pWrite)
{
     THeadLock lock(m_mutHead);      // protect the head and string table
     if (pRead)
        *pRead = m_Head.m_creator;   // copy the creator object
     if (pWrite)
     {
         m_Head.m_creator = *pWrite;
         m_bHeadDirty = true;       // needs writing
     }
     return S64_OK;
 }

//************************* V a l i d T i m e () *************************
// Check the passed in TSONTimeDate structure for validity.
// pTD      The structure to check
// returns  1 if OK, 0 if all zero, -1 if invalid fields
static int ValidTime(const TTimeDate* pTD)
{
    int iRet = 1;                           // assume all is ok
    if ((pTD->wYear < 1980) || (pTD->wYear > 2200) ||
        (pTD->ucMon < 1) || (pTD->ucMon > 12) ||
        (pTD->ucDay < 1) || (pTD->ucDay > 31) ||
        (pTD->ucHour > 23) ||
        (pTD->ucMin > 59) ||
        (pTD->ucSec > 59) ||
        (pTD->ucHun > 99))
    {
        int iZero = pTD->ucMon | pTD->ucDay | pTD->ucHour |
                                 pTD->ucMin | pTD->ucSec | pTD->ucHun;
        iRet = ((iZero == 0) && (pTD->wYear == 0)) ? 0 : -1;
    }

    return iRet;
}

// returns the maximum time in the file. If it is set in the head, use that, else
// scan all the channels to get the maximum time.
TSTime64 TSon64File::MaxTime(bool bReadChans) const
{
    TSTime64 t = -1;
    {
        THeadLock lock(m_mutHead);      // protect the head and string table
        t = m_Head.m_maxFTime;          // get the current time
    }
    
    if ((t < 0) || bReadChans)
    {
        TChRdLock lock(m_mutChans);     // we are not changing the #chans
        std::for_each(m_vChan.begin(), m_vChan.end(),
            [&t](const TpChan& p)
            {
                if (p)
                {
                    TSTime64 tChan = p->MaxTime();
                    if (tChan > t)
                        t = tChan;
                }
            }
        );
    }

    return t;
}

// You can either extend the maximum time in the file, or cancel it, making the
// file appear to have no data.
void TSon64File::ExtendMaxTime(TSTime64 t)
{
    THeadLock lock(m_mutHead);      // protect the head and string table
    if (((t<0) && (m_Head.m_maxFTime>=0)) || (t > m_Head.m_maxFTime))
    {
        m_Head.m_maxFTime = t;
        m_bHeadDirty = true;
    }
}

// pTDGet   If not nullptr, return the current one through this pointer.
//          returns +1 if fetch and OK, 0 if fetch and all zeros, -1 if invalid.
// pTDSet   If not nullptr and the date is valid or all 0, use it to set the
//          time and date field of the file. Returns 0 or -ve error.
// returns  0/1 if all OK or a SON error code.
//
// Note that pTDSet and pTDGet must NOT be the same memory or you will
// not change the data.
int TSon64File::TimeDate(TTimeDate* pTDGet, const TTimeDate* pTDSet)
{
     THeadLock lock(m_mutHead);      // protect the head and string table
     int iRet = 0;
     if (pTDGet)
     {
         *pTDGet = m_Head.m_tdZeroTick;
         iRet = ValidTime(pTDGet);  // 1=OK, 0=all zeros, -1=out of range
     }

     if (pTDSet)
     {
         if (ValidTime(pTDSet) >= 0)
         {
             m_Head.m_tdZeroTick = *pTDSet;
             m_bHeadDirty = true;       // needs writing
         }
         else
             iRet = BAD_PARAM;
     }
     return iRet;
}

// The the file version (major*256+minor).
int TSon64File::GetVersion() const
{
    THeadLock lock(m_mutHead);
    return m_Head.Version();
}

// Return the file size. Note that, for an online file, this is very much an estimate
uint64_t TSon64File::FileSize() const
{
    if (m_bOldFile)                 // Use simple method of file is offline
    {
        THeadLock lock(m_mutHead);
        return m_Head.m_doNextBlock;
    }
    uint64_t siz = 0;               // If sampling we have to take buffers into account
    TChRdLock lock(m_mutChans);     // We are not changing the #chans
    for (TChanNum c = 0; c < static_cast<TChanNum>(m_vChan.size()); ++c)
    {
        if (m_vChan[c])             // For every channel that is there
            siz += m_vChan[c]->GetChanBytes(); // Sum the amount of channel data
    }
    siz += m_Head.m_nChannels * sizeof(TChanHead); // Add in the channel headers
    siz += (m_Head.m_nHeaderExt+2)*DBSize; // Add in the overall header size
    return siz;
}

uint64_t TSon64File::ChanBytes(TChanNum chan) const
{
    TChRdLock lock(m_mutChans);     // we are not changing the #chans
    if ((chan >= m_vChanHead.size()) || !m_vChan[chan])
        return 0;
    return m_vChan[chan]->GetChanBytes();
}

//============================ Channel operations ==================================
int TSon64File::SetChanComment(TChanNum chan, const char* szComment)
{
    TChRdLock lock(m_mutChans);     // we are not changing the #chans
    if ((chan >= m_vChanHead.size()) || !m_vChan[chan])
        return NO_CHANNEL;
    m_vChan[chan]->SetComment(szComment);
    return S64_OK;
}

int TSon64File::GetChanComment(TChanNum chan, int nSz, char* szComment) const
{
    TChRdLock lock(m_mutChans);     // we are not changing the #chans
    if ((chan >= m_vChanHead.size()) || !m_vChan[chan])
        return NO_CHANNEL;
    return String2SZ(szComment, nSz, m_vChan[chan]->GetComment());
}

TSTime64 TSon64File::ChanMaxTime(TChanNum chan) const
{
    TChRdLock lock(m_mutChans);     // we are not changing the #chans
    if ((chan >= m_vChanHead.size()) || !m_vChan[chan])
        return NO_CHANNEL;
    return m_vChan[chan]->MaxTime();
}

// Get the first channel with nothing assigned or that is off.
int TSon64File::GetFreeChan() const
{
    TChRdLock lock(m_mutChans);     // we are not changing the #chans
    auto it = std::find_if(m_vChan.begin(), m_vChan.end(),
        [](const TpChan& p){return (!p) || (p->ChanKind() == ChanOff);});
    return (it == m_vChan.end()) ? NO_CHANNEL : static_cast<int>(it - m_vChan.begin());
}

int TSon64File::SetChanTitle(TChanNum chan, const char* szTitle)
{
    TChRdLock lock(m_mutChans);     // we are not changing the #chans
    if ((chan >= m_vChanHead.size()) || !m_vChan[chan])
        return NO_CHANNEL;
    m_vChan[chan]->SetTitle(szTitle);
    return S64_OK;
}

int TSon64File::GetChanTitle(TChanNum chan, int nSz, char* szTitle) const
{
    TChRdLock lock(m_mutChans);     // we are not changing the #chans
    if ((chan >= m_vChanHead.size()) || !m_vChan[chan])
        return NO_CHANNEL;
    return String2SZ(szTitle, nSz, m_vChan[chan]->GetTitle());
}

int TSon64File::SetChanScale(TChanNum chan, double dScale)
{
    TChRdLock lock(m_mutChans);     // we are not changing the #chans
    if ((chan >= m_vChanHead.size()) || !m_vChan[chan])
        return NO_CHANNEL;
    m_vChan[chan]->SetScale(dScale);
    return S64_OK;
}

int TSon64File::GetChanScale(TChanNum chan, double& dScale) const
{
    TChRdLock lock(m_mutChans);     // we are not changing the #chans
    if ((chan >= m_vChanHead.size()) || !m_vChan[chan])
        return NO_CHANNEL;
    dScale = m_vChan[chan]->GetScale();
    return S64_OK;
}

int TSon64File::SetChanOffset(TChanNum chan, double dOffset)
{
    TChRdLock lock(m_mutChans);     // we are not changing the #chans
    if ((chan >= m_vChanHead.size()) || !m_vChan[chan])
        return NO_CHANNEL;
    m_vChan[chan]->SetOffset(dOffset);
    return S64_OK;
}

int TSon64File::GetChanOffset(TChanNum chan, double& dOffset) const
{
    TChRdLock lock(m_mutChans);     // we are not changing the #chans
    if ((chan >= m_vChanHead.size()) || !m_vChan[chan])
        return NO_CHANNEL;
    dOffset = m_vChan[chan]->GetOffset();
    return S64_OK;
}

int TSon64File::SetChanUnits(TChanNum chan, const char* szUnits)
{
    TChRdLock lock(m_mutChans);     // we are not changing the #chans
    if ((chan >= m_vChanHead.size()) || !m_vChan[chan])
        return NO_CHANNEL;
    m_vChan[chan]->SetUnits(szUnits);
    return S64_OK;
}

int TSon64File::GetChanUnits(TChanNum chan, int nSz, char* szUnits) const
{
    TChRdLock lock(m_mutChans);     // we are not changing the #chans
    if ((chan >= m_vChanHead.size()) || !m_vChan[chan])
        return NO_CHANNEL;
    return String2SZ(szUnits, nSz, m_vChan[chan]->GetUnits());
}

TDataKind TSon64File::ChanKind(TChanNum chan) const
{
    TChRdLock lock(m_mutChans);     // we are not changing the #chans
    if ((chan >= m_vChanHead.size()) || !m_vChan[chan])
        return ChanOff;
    else
        return m_vChan[chan]->ChanKind();
}

TSTime64 TSon64File::ChanDivide(TChanNum chan) const
{
    TChRdLock lock(m_mutChans);     // we are not changing the #chans
    if ((chan >= m_vChanHead.size()) || !m_vChan[chan])
        return 1;                   // be kind to callers so they don't divide by 0
    else
        return m_vChan[chan]->ChanDivide();
}

int TSon64File::PhyChan(TChanNum chan) const
{
    TChRdLock lock(m_mutChans);     // we are not changing the #chans
    if ((chan >= m_vChanHead.size()) || !m_vChan[chan])
        return NO_CHANNEL;
    else
        return m_vChan[chan]->GetPhyChan();
}

double TSon64File::IdealRate(TChanNum chan, double dRate)
{
    TChRdLock lock(m_mutChans);     // we are not changing the #chans
    double dReturn = 0.0;
    if ((chan < m_vChanHead.size()) && m_vChan[chan])
    {
        dReturn = m_vChan[chan]->GetIdealRate();
        if (dRate >= 0.0)
            m_vChan[chan]->SetIdealRate(dRate);
    }
    return dReturn;
}

int TSon64File::ChanDelete(TChanNum chan)
{
    int err = Commit();             // get up to date
    if (err == 0)
    {
        TChWrLock lock(m_mutChans);     // we are changing
        if ((chan >= m_vChanHead.size()) || !m_vChan[chan])
            err = NO_CHANNEL;
        else
        {
            err = m_vChan[chan]->Delete(); // mark channel as deleted
            if (err == S64_OK)
            {
                m_vChan[chan]->Commit();    // update on disk
                m_vChan[chan].reset();      // kill off the channel object
            }
        }
    }
    return err;
}

int TSon64File::ChanUndelete(TChanNum chan, eCU action)
{
    switch (action)
    {
    case eCU_kind:
        {
            TChRdLock lock(m_mutChans);     // we are not changing the #chans
            if (chan >= m_vChanHead.size())
                return NO_CHANNEL;
            if (!m_vChanHead[chan].IsDeleted())
                return ChanOff;
            return m_vChanHead[chan].m_lastKind;
        }
        break;

    case eCU_restore:
        {
            TChWrLock lock(m_mutChans);     // we are changing
            if (chan >= m_vChanHead.size())
                return NO_CHANNEL;
            if (!m_vChanHead[chan].IsDeleted())
                return CHANNEL_TYPE;
            int err = m_vChanHead[chan].Undelete();   // restore the channel
            if (err == S64_OK)
            {
                err = CreateChannelFromHeader(chan);
                if (err == S64_OK)
                    m_vChan[chan]->SetModified();
            }
            return err;
        }
        break;
    }

    return S64_OK;
}

int TSon64File::GetChanYRange(TChanNum chan, double& dLow, double& dHigh) const
{
    TChRdLock lock(m_mutChans);     // we are not changing the #chans
    if ((chan >= m_vChanHead.size()) || !m_vChan[chan])
        return NO_CHANNEL;
    else
        return m_vChan[chan]->GetYRange(dLow, dHigh);
}

int TSon64File::SetChanYRange(TChanNum chan, double dLow, double dHigh)
{
    TChRdLock lock(m_mutChans);     // we are not changing the #chans
    if ((chan >= m_vChanHead.size()) || !m_vChan[chan])
        return NO_CHANNEL;
    else
        return m_vChan[chan]->SetYRange(dLow, dHigh);
}

int TSon64File::ItemSize(TChanNum chan) const
{
    TChRdLock lock(m_mutChans);     // we are not changing the #chans
    if ((chan >= m_vChanHead.size()) || !m_vChan[chan])
        return NO_CHANNEL;
    else
        return static_cast<int>(m_vChan[chan]->GetObjSize());
}
//==========================================================================
// Saving operations only have any effect if the channels are buffered
void TSon64File::Save(int chan, TSTime64 t, bool bSave)
{
    TChRdLock lock(m_mutChans);     // we are not changing the #chans
    TChanNum lo = (chan < 0) ? 0 : static_cast<TChanNum>(chan);
    TChanNum hi = (chan < 0) ? static_cast<TChanNum>(m_vChan.size()-1) : lo;
    for (TChanNum c = lo; c <= hi; ++c)
    {
        if (m_vChan[c])
            m_vChan[c]->Save(t, bSave);
    }
}

void TSon64File::SaveRange(int chan, TSTime64 tFrom, TSTime64 tUpto)
{
    TChRdLock lock(m_mutChans);     // we are not changing the #chans
    TChanNum lo = (chan < 0) ? 0 : static_cast<TChanNum>(chan);
    TChanNum hi = (chan < 0) ? static_cast<TChanNum>(m_vChan.size()-1) : lo;
    for (TChanNum c = lo; c <= hi; ++c)
    {
        if (m_vChan[c])
            m_vChan[c]->SaveRange(tFrom, tUpto);
    }
}

bool TSon64File::IsSaving(TChanNum chan, TSTime64 tAt) const
{
    TChRdLock lock(m_mutChans);     // we are not changing the #chans
    return (chan < m_vChanHead.size()) &&
            m_vChan[chan] &&
            m_vChan[chan]->IsSaving(tAt);
}

int TSon64File::NoSaveList(TChanNum chan, TSTime64* pTimes, int nMax, TSTime64 tFrom, TSTime64 tUpto) const
{
    TChRdLock lock(m_mutChans);     // we are not changing the #chans
    return ((chan < m_vChanHead.size()) && m_vChan[chan]) 
            ? m_vChan[chan]->NoSaveList(pTimes, nMax, tFrom, tUpto) : 0;
}

int TSon64File::LatestTime(int chan, TSTime64 t)
{
    TChRdLock lock(m_mutChans);     // we are not changing the #chans
    TChanNum lo = (chan < 0) ? 0 : static_cast<TChanNum>(chan);
    TChanNum hi = (chan < 0) ? static_cast<TChanNum>(m_vChan.size()-1) : lo;
    for (TChanNum c = lo; c <= hi; ++c)
    {
        if (m_vChan[c])
            m_vChan[c]->LatestTime(t);
    }
    return S64_OK;
}

//! Create a channel to match the header read from disk
/*!
\internal
To call this you hold the head mutex and you have just filled in the channel
headers by reading from a file. This could throw an out of memory exception.
\param chan The channel number.
\return     Always 0 (but does assert if an unknown type).
*/
int TSon64File::CreateChannelFromHeader(TChanNum chan)
{
    switch (m_vChanHead[chan].m_chanKind)
    {
    case ChanOff:
        break;

    case Adc:
        m_vChan[chan] = std::make_unique<CAdcChan>(*this, chan, m_vChanHead[chan].m_tDivide);
        break;

    case EventFall:
    case EventRise:
        m_vChan[chan] = std::make_unique<CEventChan>(*this, chan, m_vChanHead[chan].m_chanKind);
        break;

    case EventBoth:
        m_vChan[chan] = std::make_unique<CMarkerChan>(*this, chan, EventBoth);
        break;

    case Marker:
        m_vChan[chan] = std::make_unique<CMarkerChan>(*this, chan);
        break;

    case TextMark:
        assert(m_vChanHead[chan].m_nColumns == 1);
        m_vChan[chan] = std::make_unique<CExtMarkChan>(*this, chan, TextMark, m_vChanHead[chan].m_nRows);
        break;

    case AdcMark:
        m_vChan[chan] = std::make_unique<CExtMarkChan>(*this, chan, AdcMark, m_vChanHead[chan].m_nRows,
                                m_vChanHead[chan].m_nColumns, m_vChanHead[chan].m_tDivide);
        break;

    case RealMark:  // Keep columns and divide in case we ever extend this to support it
        m_vChan[chan] = std::make_unique<CExtMarkChan>(*this, chan, RealMark, m_vChanHead[chan].m_nRows,
                                m_vChanHead[chan].m_nColumns, m_vChanHead[chan].m_tDivide);
        break;

    case RealWave:
        m_vChan[chan] = std::make_unique<CRealWChan>(*this, chan, m_vChanHead[chan].m_tDivide);
        break;

    default:
        assert(false);      // add code for channel type
    }

    return 0;
}

// To call this you hold the head mutex and you have just filled in the channel
// headers by reading from a file.
int TSon64File::CreateChannelsFromHeaders()
{
    m_vChan.clear();                // no saved information
    const size_t nChans = m_vChanHead.size();
    m_vChan.resize(nChans);         // channel control stuctures
    int err = 0;
    for (TChanNum chan = 0; chan < nChans; ++chan)
    {
        CreateChannelFromHeader(chan);

        // This next is here to repair files that did not have indexes correctly written
        // Added by GPS, 15/1/2015
        if (!m_bReadOnly &&         // if file is modifiable... 
            (err < 2) &&            // ...and not said that indexes are OK
            m_vChan[chan])          // ...and if we have a channel...
            err = m_vChan[chan]->FixIndex();    // attempt to fix indices in case bad
    }
    return err < 0 ? err : 0;
}

//! Search backwards to find the nth data point before a given time
/*!
 Given a time range from tStart going backwards to tEnd we search for the nTh point before
 tStart back to (and including tEnd) for the nTh point before. If this is a waveform channel
 or an extended marker channel with data that can be treated as a waveform, we only allow
 contiguous data before (that is a gap of more than the point spacing ends the backwards
 search - but you are allowed a gap before tStart.
 \param chan    The channel to search.
 \param tStart  The end of the time range, not included in the search.
 \param tEnd    The most backward time, that is (tEnd, tStart] form a time range with tStart excluded.
 \param n       The number of points to go back (minimum of 1).
 \param pFilter Used with marker and marker derivatives to filter the search.
 \param bAsWave Used with extended marker derivatives to treat attached data as a wave.
 \return        The time or -1 if not found or a negative error code (e.g. NO_CHANNEL).
*/
TSTime64 TSon64File::PrevNTime(TChanNum chan, TSTime64 tStart, TSTime64 tEnd, uint32_t n, const CSFilter* pFilter, bool bAsWave)
{
    if (tEnd >= tStart)             // if no possibility of previous item...
        return -1;                  // ...bail out now before any locking

    TChRdLock lock(m_mutChans);     // we are not changing the #chans
    if ((chan >= m_vChanHead.size()) || !m_vChan[chan])
        return NO_CHANNEL;

    CSRange r(tEnd, tStart, n);     // make a range object to manage the request
    TSTime64 t;
    do{ t = m_vChan[chan]->PrevNTime(r, pFilter, bAsWave);} while (t == CALL_AGAIN);

    // At this point, if t < 0 for an event channel you can check if we got something
    // by seeing if r.m_nMax is less than n.
    return t;
}

//! Set the buffering used with a channel or all channels
/*!
 This is used for all existing channels, when it allocates buffering space and also saves the
 buffering time, or it is used for a single channel at a time. If you do not call this
 function, no circular buffering is applied and the Save() and SaveRange() functions will
 have no useful effect. When used for all channels:

 All: If dSeconds is >0, this is the desired buffering time. If nBytes is non-zero, it is a
      limit on the space to allow for all channels and will reduce dSeconds to fit with it. If
      you set nBytes to 0 and dSeconds > 0, the number of bytes is calculated and not limited.
      If nBytes is 0 and dSeconds is <= 0, all buffering is cancelled on all channels. In all
      cases, the used dSeconds value is saved.

 One: If dSeconds is negative, it is replaced by any saved dSeconds. Then, if dSeconds is > 0
      this sets the buffering (limited by nBytes if nBytes > 0). If dSeconds is 0, the buffering
      is set by nBytes (or removed if nBytes is 0).

 The Son32 case cleared and reallocated the channel read buffer, which we do not do as it
 is not relevant. It also set the write buffer size as long as it was not already set. If
 you change a used write buffer, you lose all the data in it, so this is not a useful thing
 to do (in most circumstances). You can set the size to 0 after committing a channel, which
 will delete the circular write buffers (if they exist).

 \param chan    Either -1 for all channels or a channel number n the file.
 \param nBytes  0 or a maximum buffer size
 \param dSeconds The desired buffering time, can be 0 or negative (see above).
 \return        The achieved buffering time. Read only files always return 0.
*/
double TSon64File::SetBuffering(int chan, size_t nBytes, double dSeconds)
{
    if (m_bReadOnly)
        return 0.0;

   TChRdLock lock(m_mutChans);      // we are not changing the #chans
    if (dSeconds < 0.0)             // If we are to use the saved buffering time...
        dSeconds = m_dBufferedSecs;

    if (chan >= 0)
    {
        if ((static_cast<size_t>(chan) >= m_vChanHead.size()) || !m_vChan[chan])
            return NO_CHANNEL;
        size_t nObjSize = m_vChan[chan]->GetObjSize();  // bytes per item
        if (dSeconds > 0)
        {
            double dIdealRate = m_vChan[chan]->GetIdealRate();
            double dSpace = nObjSize * dIdealRate * dSeconds; // predicted space
            if ((nBytes == 0) || (dSpace < nBytes))
                nBytes = static_cast<size_t>(dSpace);
            dSeconds = (dIdealRate > 0) ? nBytes / (dIdealRate*nObjSize) : 0;
        }

        if ((m_vChan[chan]->WriteBufferSize() == 0) || (nBytes == 0))
            m_vChan[chan]->ResizeCircular(nBytes / nObjSize);
    }
    else
    {
        double dBytesPerSec = 0.0;              // initialize the total bytes per sec
        std::for_each(m_vChan.cbegin(), m_vChan.cend(),
            [&dBytesPerSec](const TpChan& p)
            {
                if (p)
                    dBytesPerSec += p->GetObjSize()*p->GetIdealRate();
            }
        );

        // If this needs too much space, or no seconds given, calc equivalent seconds.
        if ((dSeconds <= 0.0) ||                // if no time supplied, or...
            (dBytesPerSec*dSeconds > nBytes))   // ...too much space needed, then scale to available space.
            dSeconds = (dBytesPerSec > 0.0) ? nBytes / dBytesPerSec : 0.0; // Beware zero rates

        std::for_each(m_vChan.begin(), m_vChan.end(),
            [dSeconds, nBytes](const TpChan& p)
            {
                if (p)
                {
                    if ((p->WriteBufferSize() == 0) || (nBytes == 0))
                    {
                        size_t n = static_cast<size_t>(p->GetIdealRate()*dSeconds);
                        p->ResizeCircular(n);
                    }
                }
            }
        );

        m_dBufferedSecs = dSeconds;             // Save buffering time
    }
    return dSeconds;
}

//! reset a previously used channel so it can be reused
/*!
 You must already hold the m_mutChans lock to use this.

 We are about to reuse a channel. You can only do this if the channel has been
 deleted. Once this operation is complete, you cannot undelete the channel. We
 release all the channel resources. If there were used blocks we will now be
 re-using them.
*/
int TSon64File::ResetForReuse(TChanNum chan)
{
    if (m_bReadOnly)
        return READ_ONLY;
    if (chan >= m_vChanHead.size()) // this is a really bad error!
        return NO_CHANNEL;
    return m_vChan[chan] ? m_vChan[chan]->ResetForReuse() : S64_OK;
}
