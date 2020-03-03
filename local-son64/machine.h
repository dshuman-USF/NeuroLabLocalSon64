/*****************************************************************************
**
** machine.h
**
** Copyright (c) Cambridge Electronic Design Limited 1991, 1992, 2010
**
** This file is included at the start of 'C' or 'C++' source file to define
** things for cross-platform/compiler interoperability. This used to deal with
** MSDOS/16-bit stuff, but this was all removed in Decemeber 2010. There are
** three things to consider: Windows, LINUX, mac OSX (BSD Unix) and 32 vs 64
** bit. At the time of writing (DEC 2010) there is a consensus on the following
** and their unsigned equivalents:
**
** type       bits
** char         8
** short       16
** int         32
** long long   64
**
** long is a problem as it is always 64 bits on linux/unix and is always 32 bits
** on windows.
** On windows, we define _IS_WINDOWS_ and one of WIN32 or WIN64.
** On linux we define LINUX
** On Max OSX we define MACOSX
**
*/

#ifndef __MACHINE_H__
#define __MACHINE_H__
#include <float.h>
#include <limits.h>

/*
** The initial section is to identify the operating system
*/
#if (defined(__linux__) || defined(_linux) || defined(__linux)) && !defined(LINUX)
#define LINUX 1
#endif

#if (defined(__WIN32__) || defined(_WIN32)) && !defined(WIN32)
#define WIN32 1
#endif

#if defined(__APPLE__)
#define MACOSX
#endif

#if defined(_WIN64)
#undef WIN32
#undef WIN64
#define WIN64 1
#endif

#if defined(WIN32) || defined(WIN64)
#define _IS_WINDOWS_ 1
#endif

#if defined(LINUX) || defined(MAXOSX)
    #define FAR

    typedef int BOOL;       // To match Windows
    typedef char * LPSTR;
    typedef const char * LPCSTR;
    typedef unsigned short WORD;
    typedef unsigned int  DWORD;
    typedef unsigned char  BYTE;
    typedef BYTE  BOOLEAN;           
#endif

#ifdef _IS_WINDOWS_
#include <windows.h>
#endif

/*
** Sort out the DllExport and DllImport macros. The GCC compiler has its own
** syntax for this, though it also supports the MS specific __declspec() as
** a synonym.
*/

#ifndef DllExport
#ifdef _IS_WINDOWS_
    #define DllExport __declspec(dllexport)
    #define DllImport __declspec(dllimport)
#else
#ifdef GNUC
    #define DllExport __attribute__((dllexport))
    #define DllImport __attribute__((dllimport))
#else
    #define DllExport
    #define DllImport
#endif
#endif
#endif /* _IS_WINDOWS_ */

    
#ifndef TRUE
   #define TRUE 1
   #define FALSE 0
#endif

#endif
