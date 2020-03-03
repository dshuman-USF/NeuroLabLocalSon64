//! \file
//! \brief Doxygen documentation file for the 64-bit son library (do not include)
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

#include "s64priv.h"
#include "s64chan.h"
#include "s32priv.h"
#include "s64filt.h"

namespace ceds64        // Horrible hack so that the doc "sees" the ceds64 namespace
{
/*! \mainpage 64-bit SON library

\section Overview Overview
The file structure is based upon the concepts of the original 32-bit SON filing system
(henceforth \ref Son32) and is designed as a successor, having the ability to hold all
the data types of the original, but removing some of its limitations. Son64 format
features include:

- Stores multiple independent channels of data that share a common time base
- Data is efficiently indexed by time and channel data is written in time order
- Two basic channel classes: waveforms sampled at a fixed interval (with optional gaps),
  time stamps with optional attached data.
- Waveform sample interval and alignment is set per channel.
- Time stamp data can have marker codes which allow data filtering on read.
- Uses 64-bit integer times, for constant time accuracy
- Designed for efficient real-time use and fast location of data
- Designed to support very large files
- Implements optional write data buffering to allow selective and retrospective choice
  of data to save.
- Supports multi-threaded use for data read and write.

The system is intended to hold raw and processed data that is written in temporal order
per channel (you cannot go back and add data to a channel to fill in gaps). However, you
can write new channels at a later time. You can delete and re-use channels within the
file and there is limited support for overwriting existing waveform data (to support
filtering) and to overwrite existing marker data (to support changes to marker codes).

\subsection Licence Software licence
Son64 is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Son64 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with SON64. If not, see <http://www.gnu.org/licenses/>.

The majority of users of the Son64 libarary will be academics and for them the terms
of the GPL should not be onerous. However, if you wish to make commercial use of the
library without the GPL requirements of publishing source code you should contact
Cambridge Electronic Design Ltd to obtain a licence for commercial use. You can
find contact information on our web site: <http://ced.co.uk/>

\subsection Dependencies Code dependencies
The library is written in C++11 (as supported by Microsoft Visual Studio 2017). More
accurately, it builds in VS2017 with the default language settings plus /permissive-
which removes many of the Microsoft incompatibilities. It should build in C++14 and
in C++17 (so let us know of incompatibilities).

It makes use of Boost for Boost bimap. To build applications that include s64priv.h
you will need to have Boost installed on your system. The Boost library is available
as a free download at <http://www.boost.org>.

We use Boost bimap for rapid lookup of strings in the string table where we need to
locate strings by id or by the string content. This is a header-only library.
If you want to remove dependencies on Boost, you will have to recode the string_store
to use a different mechanism to do bidirectional maps. The data saved on disk for the
string_store does not depend on this in any way.

We have built the Son64 library in Windows using Boost versions 1.52 to 1.64 and
will continue to check that it works with the latest Boost (as long as it depends
on Boost).

\subsection Son64 Son64
When we refer to Son64 files, we mean the physical data files with the standard `.smrx`
file extension that use 64-bit times to store channels of data. This is the native disk
format of the Son64 library described by this documentation. All features of the 
software described in this documentation are available to these files.

When we refer to the Son64 library, we mean the software interface described in this
document that allows us to read and write Son64 data file. However, it also allows us
to read and write \ref Son32 files with the same code. It could also read and write
other physical file formats if the necessary software work were done to enable this.

The 64 in the name refers to the number of bits used to save a time. It _does not_ 
relate to the library being compiled to run on a 64-bit (x64) or a 32-bit CPU (x86).
The library can be compiled to run on either CPU size in Windows.

\subsection Son32 Son32
When we refer to Son32 files, we mean an old format `.smr` data file using 32-bit 
times. We emulate the new library interface with these files and when reading them,
you will find that they behave identically to Son64 files. However, there are
differences when writing:

- You cannot write items at times greater than #TSTIME_MAX (0x7fffffff)
   clock ticks
- The files are limited in size to 1 TB (big files) or 2 GB
- Text strings have fixed size limits
- There are fewer file comments available
- Level event channels are implemented differently (though you should not notice this)
- Channel write buffering is at at the granularity of disk buffers with less control over
  saving and discarding data

\subsection comp6432 Comparison of Son64 and Son32
This table compares some of the features of the 64 and 32-bit filing systems.

Feature	                       |New 64-bit system    |  Old 32-bit system
-------------------------------|:-------------------:|:-------------------:
Max file time at 1 us tick	   | 292,471 years       | 35 minutes, 47 seconds
Max file time at 1 ns tick     | 292 years           | 2.147 seconds
Max file time at 1 ps tick     | 106 days            | 2.147 ms
Maximum file size              | 16 EB (16 x 10^18)  | 2 GB or 1 TB (big file)
Minimum channels               | 32                  | 32
Maximum possible channels      | 65534               | 451
Max channels allowed           | 1000                | 400
Comments, titles, units        | Variable lengths    | Fixed lengths
Bytes per event                | 8                   | 4
Bytes per marker               | 16                  | 8
Bytes per wave point (int,real)| 2,4                 | 2,4
Waveform gap overhead          | 16 bytes            | to the end of the block
Data block size                | 64 kB               | usually 32 kB
Random access time (huge file) | O(ln(channel size)) | O(channel size)
Sequential access time         | O(1) amortised      | O(1)
Undelete channel               | Yes                 | Not easily
Long reads block writes        | No (limited block)  | Yes

Although the new format uses double the space to store times, waveforms (which often
account for the majority of the disk space) occupy much the same space (and can occupy
less if there are gaps in the waveform data). So data file sizes can be
broadly similar if there are few time stamps compared to waveform points. If the time
stamps predominate, then Son64 files will be up to double the size.

\section ClockTick Clock ticks and base time units
The most basic concept is that of the indivisible unit of time, the clock tick or time
base. The clock tick period in seconds is set with CSon64File::SetTimeBase()
and retrieved with CSon64File::GetTimeBase(). The time base is usually of order 1
microsecond (1.0e-6 seconds) and often represents the timing resolution of the hardware
device used to sample the data. As hardware improves, the time base becomes shorter. Old
versions of the 32-bit filing system limited the time bases to integer microseconds, but
the current (final) Son32 system allows arbitrary time bases.

You can change the file time base, and this will scale the time of all channel data in
the file.

Time is measured in multiples of this clock tick, using a 64-bit integer representation,
the #TSTime64 type, a synonym for the standard int64_t type. The standard INT64_MAX, the
largest signed integer representable in 64 bits, is 2^63 - 1. This is such a large value
that we define the #TSTIME64_MAX (the largest time we expect to see) as 
INT64_MAX - (INT64_MAX/8) to give us arithmetic headroom. This avoids the necessity to
test for arithmetic overflow when adding times less than INT64_MAX/8 to a #TSTime64.

The emulation of this in \ref Son32 files limits us to 32-bit numbers, which means these
files are somewhat constrained in length (at 1 us per tick, a Son32 file can only be some
35 minutes long). A \ref Son64 file has no such constraint; at 1 ns per tick a Son64 file
could be 292 years long.

\subsection ClockTickDouble Timing accuracy and floating point numbers
The library handles all times as #TSTime64 (int64_t), which means that times and time
differences are as accurate at the start of a file (close to 0) as they are the end.
However, users mostly like to work with times in seconds and these are conveniently
expressed as floating point double precision (IEEE 64-bit floating point) numbers.

The 64-bit floating point doubles have effectively 53 bits of precision. This means that
if you use floating point arithmetic to convert times in seconds into ticks, the result
starts to lose precision at approximately 9e15 ticks. By losing precision we mean that
the next representable time as a double after 2^53 ticks is at 2^53+2. At 2^54 ticks,
the next representable tick is 4 ticks later, at 2^55 it is 8 ticks, and so on.

This is not a problem for normal use (at 1 microsecond per tick this happens at a file
time of 292 years). However, if you run at a much higher time resolution it might be
important. For example, at 1 nanosecond per tick this happens at a file time around
100 days. At 1 picosecond per tick it happens around 150 minutes.

However, in most circumstances, algorithms that deal with times use differences of
absolute times over relatively short periods. Should this effect be a problem, you can
almost always eliminate it by handling time differences with #TSTime64 integers (which
give the same precision regardless of the position in the file), and then converting to
floating point.

_Never_ use single precision floating point (float) types for time calculations. These
have 24 bits of precision. At 1 microsecond per tick, these start to lose precision
compared to #TSTime64 after 16.777216 seconds.

\section ChanTypes Data channel types
For reasons of compatibility, we support all the old format data types. These can be
grouped as:
- equally spaced waveforms
  - Adc data (stored as 16-bit integers)
  - RealMark (stored as 32-bit floating point)
- time stamps based items
  - EventRise and EventFall (times)
  - Marker data (time plus marker codes)
    - EventBoth (Marker data coded as high/low)
    - TextMark (Marker data plus text)
    - RealMark (Marker data plus a list of real values)
    - AdcMark (Marker data plus waveform fragments - spike shapes)

This family tree of data channel types is slightly different for \ref Son32 files where
the EventBoth type is not a marker, being stored as times with the high/low alternating
between consecutive elements. In this case, the EventBoth type should be moved up one
line and left to align with EventRise and EventFall.
    
\subsection Waveform Waveform data
\image html waveform.svg
The waveforms you record are continuously changing voltages. We stores waveforms as either
a list of 16-bit signed integers (#Adc) or 32-bit floating point numbers (#RealWave) that
represent the waveform amplitude at equally spaced time intervals. We also store a scale
factor and offset for each channel to convert between integers and user defined units,
the value of the data in user units is given by:

real value = integer * scale / 6553.6  +  offset

This scaling came about for historical reasons. Back around 1970 it was very common to
have 5 Volt ADC systems where the range of the inputs was from -5.0000 to + 4.9998 Volts,
which was mapped into the 16-bit integer range -32768 to 32767. This means that a scale
factor of 1.0 and an offset of 0 produced a real value in Volts.

The scale and offset allow us to read RealWave data as Adc and Adc data as RealWave. If
you read a RealWave channel as Adc data, and the result exceeds the Adc data range, the
values returned are limited to the range -32768 to 32767.

The process of converting a waveform into a number at a particular time is called
_sampling_. The time between two samples is the _sample interval_ and the reciprocal of
this is the _sample rate_, which is the number of samples per second. The sample rate
for a waveform must be high enough to correctly represent the data. It can be
demonstrated mathematically that you must sample at a rate more than double the highest
frequency contained in the data to be able to reconstruct the data. On the other hand,
you want to sample at the lowest frequency possible, to minimise storage space and
processing time. The library allows you to save different waveform channels at different
rates; you can choose rates appropriate to the channel data.

Waveform data is sampled at an integer multiple of the file clock tick period and this
divisor can be different for each channel, thus all the waveform channels can be sampled
at different rates. Each block of waveform data holds a start time, so waveform data need
not be continuous. Two waveform blocks on the same waveform channel hold continuous data
if the time interval between last sample in the first block and the first sample of the
second block is equal to the sample interval. It is an error for data blocks to overlap.

Spike2 (a major producer and consumer of Son data files) further assumes that one a 
waveform channel has been written to, all further data will be written at times that are
integer multiples of the sample interval for that channel.

\subsection EventFallRise Event rise and Event fall data
\image html event.svg
These two channel types (#EventFall, #EventRise) are identical as far as the Son filing
system is concerned. The names are derived from whether the edge of the hardware signal
that generated them was falling or rising. They are stored as 64-bit integer multiples of
the underlying file clock (the #TSTime64 type).

\subsection MarkerData Marker data
\image html marker.svg
Marker events (#Marker) are stored as 64-bit times, like events. They also have 8
additional bytes of information associated with each time. These additional bytes hold
information about the type of each marker. Currently we divide the extra 8 bytes into 4
marker codes (to match the \ref Son32 system) and a 4 byte integer that is currently
unused and reserved for future use. If you read \ref Son32 Marker data, the extra bytes
will always hold zeros.

Markers can be filtered based on the marker codes by a user-supplied mask so that only
those markers which meet a set of criteria will be considered. The library functions can
read a Marker channel as an Event channel or as a Marker channel.

\subsection LevelEvent Level event data
This is an event type (#EventBoth) that represents an input that can be in one of
two states (conceptually low and high, but this could be off and on, red or green,
or whatever is appropriate for your data). In the \ref Son32 system, this was
stored identically to EventFall and EventRise data with a marker in each data block
to indicate the polarity of the first event in each block and the state was assumed
to alternate.

In the \ref Son64 system, Level event data is stored as \ref MarkerData with the
first marker code defining which type of edge this is. Code 0 means the data is
now low, any other code (though 1 is preferred) means it has gone high.

The Son64 library lets you read this data as alternating event times, or as markers.

\subsection AdcMarkData AdcMark data
\image html adcmark.svg
This type (#AdcMark) is stored as \ref MarkerData followed by 1 or more interleaved
traces of waveform data. The waveform(s)
typically holds a transient shape, and the marker bytes hold classification codes
required for the transient. The first point in the (first trace of) waveform data
is sampled at the marker time. The library functions can use an #AdcMark channel
as if it were a waveform, Event, Marker or AdcMark channel.

When there are multiple traces, the data is organised in memory by time and trace.
If we have AdcMark data with 4 traces A, B, C, D:
\code
A0 A1 A2 A3 A4 A5...
B0 B1 B2 B3 B4 B5...
C0 C1 C2 C3 C4 C5...
D0 D1 D2 D3 D4 D5...
\endcode
These would be arranged in memory as:
\code
A0 B0 C0 D0 A1 B1 C1 D1 A2 B2 C2 D2 A3 B3 C3 D3 A4 B4 C4 D4 A5 B5 C5 D5...
\endcode
This matches the C declaration:
\code
short sData[4][pointsPerTrace];
\endcode

\subsection RealMarkData RealMark data
\image html realmark.svg
This type (#RealMark) is \ref MarkerData with attached real numbers. It is stored as
a Marker, followed by an array of 32-bit IEEE real numbers. The library functions
can use a RealMark channel as if it were an Event, Marker or RealMark channel. The
library can also treat the attached data as a two-dimensional matrix, like
multi-trace AdcMark data, but the main user of this data type (Spike2) assumes
a linear array of data (and would likely behave unexpectedly with a matrix).

\subsection TextMarkData TextMark data
\image html textmark.svg
This type (#TextMark) is \ref MarkerData to which is attached a character string. It
is stored as Marker, followed by a fixed-size array of characters. The strings
stored in the arrays are terminated by a zero byte. However, the library does not
inspect written data to ensure that this is the case, so application code to
extract a string from a TextMark item should stop copying data at the first zero
or at the maximum stored string size, whichever is the smaller. The library
functions can use a TextMark channel as if it were an Event, Marker or TextMark
channel.

\section FutureWork Future developments
The Son64 system is designed with future extensions in mind:

\subsection FutureChanType Possible new channel types
The channel types that currently exist are ones selected for backwards compatibility with
the \ref Son32 format. However, this does not preclude the addition of more types. For
example, if it were essential to add waveforms with IEEE 64-bit doubles rather than the
current 32-bit float data, this would be a relatively easy extension. Also, the extended
markers can actually cope with just about any form of fixed-size data packet.

At a more ambitious level, it would be possible to store multimedia data in the file. A
waveform channel already stores lumps of contiguous waveform data with gaps and it would
not be too hard to subvert this to store video or audio data.
        
\subsection compressedWave Compressed waveform data for display
Programs like Spike2 can get slow when displaying huge quantities of waveform data due
to the time that is needed to transfer the data from the disk file. When displaying such
data, all that is required is to give an impression of the range of the data. To speed
up such operations we may add a compressed wave type where we store a wave as a maximum
and a minimum at perhaps 1/1000 of the original sampling rate (or at whatever rate makes
sense in terms of draw time).

There could even be multiple compresed versions (1:1000,
1:1000000, etc.) That is, when disk read time causes display of compressed data to get
slow, we change over to asking for compressed data, and if we have it, that is what we 
return, rather than the raw data. For example, if the raw waveform data on the screen
needs 1 GB of disk space, this would take many seconds to read, but reading a 1000:1
compressed version would take much less.

\section TooMuchInfo What do I need to read?
The documentation you are reading was produced with DOxygen <http://www.doxygen.org> and
contains much more information than you need to use the filing system. You can totally
ignore anything in the ceds32 namespace (this is the source code documentation for the
original library, which is included in the Son64 library, but used through the Son64 API).

You only need to know about types and classes that are referenced by the routines defined
in the file s64.h. These routines are overloaded by the TSon64File and TSon32File classes,
and these have internal types that are used. However, if something is not exposed by the
CSon64File class interface, you have no need to know about it.

You may be interested to read about the \ref FileStruct but this is not required to use
the library. You probably should read the \ref LibInt section (it's not too long).

You will want to read the sections: \ref ReadFile, \ref WriteData and \ref FileReadOnly
as these hold lots of examples to get you started.

If you intend to write data as it is sampled you will very likely want to know about
\ref PgWrBuf.

There is also the Modules section. This groups the available routines into functional
groups.

*/

//--------------------------------------------------------------------------------------

/*!
\page FileStruct Son64 file structure
\tableofcontents
This section describes how the file system is implemented on disk. You do not need to
know this information to use the API to read and write data. However, having some
knowledge of how the system stores data may help your expectations of the performance
you can expect (and not expect) from the system.

You will need to know and understand the information here if you want to write code
to fix damaged files or you need to debug problems in the library or extend the
library functionality.

This library can read and write the Son32 format. However, this document does not
discuss the \ref Son32 file structure on disk. That disk format is covered in
the original SON library documentation, available from CED.

The data stored on disk is stored in little-endian format. That is, if the first 8
bytes of a file hold a 64-bit integer, byte 0 holds the least significant byte and
byte 7 holds the most significant.

The library can be configured for a range of data block size (#DBSize) and lookup
block size (#DLSize); the descriptions that follow are for the standard sizes of
64 kB (#DBSize) blocks with 4 kB (#DLSize) lookup blocks. All files created by
Spike2 uses these parameters, which are a reasonable compromise of performance
vs memory usage. The file parameters are encoded in the file header, see
\ref DiskBlockHeadValues for details.

\section fileBlocks The basic DBSize blocks in a Son64 file

\image html blocks.svg "A Son64 file is a sequence of fixed size blocks"

A Son64 file is composed of #DBSize (64 kB, 65536 bytes) fixed size blocks. These blocks
all start with a 16 byte \ref DiskBlockHead that identifies the block, the channel it
belongs to, the parent block that owns it and the block type.
There are three types of #DBSize data blocks in a file:

1.  __Header__: These blocks are identified by a channel number of 65535. The header
    blocks are normally contiguous (but need not be) and the first is always at the
    start of the file. The structure is defined by TFileHead which starts with a
    \ref DiskBlockHead.

2.  __Index__: These are an array of #DBSize/#DLSize (16) TDiskLookup blocks, identified
    by a channel number (0 to 65534) and a level code in the range 1 to 6. Each Lookup
    block is a \ref DiskBlockHead followed by an array TDiskTableItem[#DLUItems].
    #DLUItems is 255 with the standard settings for #DBSize and #DLSize. 

3.  __Data__: These hold channel data identified by a channel number (0 to 65534) and a
    level code of 0. The structure is defined by TDataBlock which starts with a
    \ref DiskBlockHead.

\subsection fileDataRecovery Recovering damaged files
Son64 (.smrx) files will often be very large and contain valuable data. By having a very
regular data structure with some redundancy built into it, we make it relatively easy to
recover a damaged or incomplete file.

Index blocks do not hold channel data. They hold information that allows us to locate
channel data efficiently, so they can be ignored when recovering a damaged file.
By reading #DBSize (64 kB) chunks of the file, we can detect header and data blocks and
ignore lookup blocks, allowing us make a copy of the file, even if the lookup tree
has become damaged.

The S64Fix program (part of the Spike2 application) recovers files in this way. If
the file header is damaged, it can use the file header from another similar file to
aid in the recovery process.

\subsection fileRealTimeEfficiency Real-time efficiency
The structure and libraries are organised so that when writing real-time data, most
writes to the file are sequential (minimising mechanical disk head movement).
The library buffers channel data in memory using user-configurable buffers.
When the buffers become full, data is appended to the end of the file.
Index blocks are allocated as required; we need a new one 
every #DLUItems * #DBSize / #DLSize data blocks, which is every 4080 data blocks (255 MB
of data) with the standard settings. The result is that when streaming multi-channel
real-time data to disk, most writes are sequential (at the end of the file) with
occasional non-sequential writes to fill in index blocks.

You can configure data buffering so that recently sampled data is available for
reading without accessing the file on disk. In most data capture scenarios, the data
that is required for display is the most recent, so this strategy minimises disk head
movement in large data files. Of course, there may be other processes using the same
physical disk over which we have no control. If you are working at the limit of data
transfer rates for your hardware you may want to consider having a dedicated data
drive.

\subsection DiskBlockHead TDiskBlockHead structure
A TDiskBlockHead header identifies every #DBSize and #DLSize block in the file. The header
is 16 bytes long, or 2 64-bit words. The image shows the structure of this header for
the standard sizes of #DBSize and #DLSize.

\image html header.svg "TDiskBlockHead structure that starts every block"

The first 8 bytes (64 bits) store the file offset of the parent of the block. Header
blocks set all 8 bytes to 0 except the first header block which uses the first 8 bytes
differently (see \ref DiskBlockHeadValues).

The #DBSize blocks are always on a 64 kB boundary, so the bottom 16 bits of the offset
are 0. The 16 lookup blocks in an Index block all lie on 4 kB boundaries, so the bottom
12 bits of these file offsets are 0. We take advantage of these known 0 bits to store
additional information (the __sub__ and __level__ fields).

\par File offset of the parent of this block
This is used by Data and Lookup blocks to indicate the file offset of the block that
'owns' this block. The first TDiskLookup block of a channel has this set to 0 as it
has no parent.

\par sub
This 4-bit field extends the parent file offset to point at a TDiskLookup block.
It is used by Data blocks and by TDiskLookup blocks. It is 0 for the first TDiskLookup
block of a channel (as it has no parent lookup block). Put another way, bits 63-12
followed by 12 0 bits is the file offset to the parent lookup block.

\par level
Bits 8-10 are used by data and index blocks to encode the block level. A level
0 block is a data block. A level 1 or more block is an index block, used to locate
data.

\par index
The bottom 8 bits is used by index and data blocks to encode the index into the parent
block that points at this block. This index should be in the range 0 to 254.

\par channel #
All blocks are either header blocks with a channel number of 0xffff (65535) or they
belong to a channel as either a lookup block or a data block, and this is set to
the channel number.

\par channel ID
When space is allocated to a channel, it is never released. As channels are written in
time order, this means that disk space for channel data is always allocated in ascending
file offset order (which is useful information when recovering a damaged file).
When a channel is first used, the channel ID is set to 0.
\par
If a channel is deleted, the space is left allocated, as is the tree of index blocks used
to locate the channel data efficiently. If the channel is later reused, the channel ID is
increased by 1. When a Data block is reused, the new channel ID is written to it. This
allows us to differentiate between the current data in a reused channel and older data
from a previous use when recovering a damaged file.

\par Count of items in the block
This is used by data blocks and by lookup table blocks. This is always non-zero except
for header blocks, when it is always 0.
\par
When used in a lookup table, all counts except for the last in each channel one should
hold #DLUItems (255) indicating that they are full. When a channel is reused, the count
of items does not change (the number of reused blocks indicates the current reuse
index).
\par
When used by data blocks, this is the number of items in the block. For event and
marker-based channels, all blocks except the last will hold the maximum number of
items that will fit in a block. However, the items counted for wave-based channels
(Adc and RealWave) are the number of contiguous sections in the block, so this
number may vary.

\section fileHeadBlocks Header blocks
The header starts with a TFileHead structure that holds basic
information about the file (such as the number of channels), the positions of any other
header blocks in the file). It is followed on disk by the optional user area, then the
channel description area and finally by the string table (an area that holds all the
strings used by the file).

The first header block starts the file, the remainder can be scattered through the
file. The routines for reading and writing the header hide the scattering. There is
a maximum number of header extension blocks set by the HD_EXT constant (currently
128). This allows for up to 8 MB of file header. We set a reasonable size for the
string table (the most likely growth region unless extra channels get added) in the
hope of avoiding header extensions.

\image html headblks.svg "File header as used by the code and as written to disk"
The picture show how the programmer treats the data as a set of objects, being the TFileHead,
the user area, the array of TChanHead structures and the string table. The TFileHead holds
offsets to the various data items as if they were laid out linearly. However, these are then
written to disk and broken up into blocks of size #DBSize, each with a header. For illustration
we imagine that the header needs to be split into two blocks (it could all fit in one or need
several).

\subsection DiskBlockHeadValues TDiskBlockHead values set in header blocks
The very first header block block replaces the parent block offset (as it has no parent)
with the file identifier string "S64xy" and revision information, where 'x' and 'y' are
codes for the data (#DBSize) and lookup table (#DLSize) block sizes used in the file.
The TSon64File::Open() checks this identifier as part of the tests it does to accept
a data file.

The x value codes the #DBSize value (64kB) as a power of 2 (which is 16), with 'a' for 1,
'b' for 2 and so on. The y value codes the lookup table size #DLSize, which is 4096, so
has a value of 12.

For the standard configuration, the first 6 bytes of a Son64 file hold: "S64pl" followed
by a zero byte. byte 7 is the minor revision of the file system and byte 8 is the major
revision (changed for incompatible format changes). The file revision is set in the
source code as REV_MAJOR (currently 1) and REV_MINOR (currently 0).

In all header blocks, the channel number is set to 0xffff (65536), and the channel ID and
item count are set to 0. Expansion header blocks also set the first 8 bytes of
TDiskBlockHead to 0.

\subsection UserArea The user data area
When you create a file you can specify the user data area size (usually 0). We allow you to
set a size up to 64 kB, but this is an arbitrary limit (and could be increased). No standard
CED software currently makes any use of this area. The use of this area is entirely up
to the application programmer. It is accessed with CSon64File::SetExtraData() and
CSon64File::GetExtraData() routines. If you make use of this area you should make
sure that you set a creator with the CSon64File::AppID() command to identify the
format of the extra data.

\subsection ChannelArea The channel description area
When you create a file, you state the maximum number of channels that it can contain. Each
channel is then allocated a TChanHead structure and an array of these is written to the
file head after the user data area.

For each used channel, the TChanHead holds the disk offset of the TDiskLookup block that
is the top of the tree that leads to the data blocks for that channel. The offset is set
to 0 if there are no data blocks for the channel.

\subsection StringTable The string table and text storage
Unlike the \ref Son32 file format, all strings that are saved in Son64 files can be any
reasonable length. The strings in a Son64 file are all identified by a string index and the
actual strings are held in a string table that is part of the file header. There is the
practical limit that all the strings must fit in the available file header space, but this
is an 8 MB area, so unlikely to be exceede by other than by determined abuse. Most
application programs that use file and channel strings will impose their own limits. For
example Spike2 limits the lengths of comments, titles and channel units.

The string table is managed by the string_store class and you can find documentation for the
string table disk format in the string_store::BuildImage() function. Unless you have a
deep interest in the inner workings of the library, you will _never_ need to look at
the string_store class documentation.

\section Indexing Index blocks and the lookup blocks they contain
A channel has Data blocks and TDiskLookup blocks. When a channel needs a new lookup
block, it either gets the next unused one from the last-allocated index block, or if none
are free, a new index block holding 16 TDiskLookup blocks is allocated and the first is
given to the channel. The TDiskLookup blocks within an index block will usually belong to
a mix of channels.

\subsection DiskLookup TDiskLookup block structure
Each TDiskLookup block holds #DLUItems (255) TDiskTableItem structures, each of 16 bytes.
The TDiskTableItem structures hold the disk offset of a block belonging to the channel
of the next level down and the time of the first item in that block.

\image html lookup.svg "TDiskLookup blocks"

The channel header TChanHead.m_doIndex fields holds the file position of the first
TDiskLookup block for the channel (or 0 if there are no blocks).

\subsection DiskLookupBuild Creating the lookup tree
A channel starts life with m_doIndex holding 0 and has no lookup blocks. When the first
data block is written, a lookup block at level 1 is created with the first item 
pointing at the first data block. The TDiskBlockHead on the data block is set to point
back at the TDiskLookup block on disk and the index within it that points at the data.
The channel header m_doIndex is set to point at the lookup block.
\image html index.svg

As each new data bock is added, the lookup block is extended to point at it. In the
image below, you should imagine that we have just written the 255th block to fill
the first TDiskLookup block. At this point, we have written around 16 MB of data to the
channel.
\image html index1.svg

Once the lookup block is full, the next block allocation causes two new lookup blocks
to be created, one for a new lookup level (2) and one to expand the original level (1).
The m_doIndex in the channel header is set to point at the new level 2 table, and this
points at both the level 1 tables, which in turn point at the level 0 data.
\image html index2.svg

We can now continue adding blocks. Each time we fill a level 1 block, we add a new level
1 block and put a pointer to it in the level 2 block. This continues until we have filled
the level 2 lookup, which happens when we have written #DLUItems*#DLUItems data blocks. At
this point we have written a little over 4 GB to the channel.

At this point we create 3 lookup blocks, one for level 3, one for level 2 and one for
level 1. We set the channel header m_doIndex to point at the level 3 lookup block, which
in turn points at the two level 2 blocks below it in the tree. We can then go on adding
data until we have written #DLUItems*#DLUItems*#DLUItems blocks at which point we create 4
more lookup blocks at levels 4, 3, 2 and 1. At this this point, we have written around
1 TB of channel data.

And so it continues until we have written 276 TB to the channel when we add
level 5. We seriously doubt that anyone will generate this much data (at least in the
next several years). There is still a factor of #DLUItems*#DLUItems more channel space
available to the determined experimenter...

You can imagine this as a tree. At the bottom, there are data blocks (these are at level
0).

Above each group of #DLUItems data blocks is one TDiskLookup block that points at these
blocks. These blocks are at level 1.

If there are more than #DLUItems data blocks, there will be another level of TDiskLookup
blocks (level 2) that point at the level 1 blocks. And so on.

As the level increases, the number of data blocks that can be stored in the file
increases as #DLUItems (255) to the power of the level. The idea is that the amount of
work to write data is little more that the work to write the data. If we keep the tree
of lookups to the current data block in memory and only write them to disk when we must
create a new lookup block, then every #DLUItems data blocks we must write a level 1
lookup block, every #DLUItems^2 blocks we must write a level 2 lookup block, every
#DLUItems^3 blocks, we write a level 3 lookup block and so on.

\subsection DiskLookupSizes Channel data size vs lookup tree depth
This table shows the number of data blocks we can accomodate at each level and the
size of the channel data in bytes that corresponds with this block count for the
standard library settings of #DBSize = 64 kB and #DLSize = 4 kB.

Level   | 1       | 2      | 3      | 4      | 5      | 6      | 7
:-------|:-------:|:------:|:------:|:------:|:------:|:------:|:------:
Blocks  | 255     | 65025  | 1.66e6 | 4.2e9  | 1.08e12| 2.74e14| 7.01e16
Data    | 16.6 MB | 4.26 GB| 1086 GB| 276 TB | 70.6 PB| 16 EB  | 16 EB

Note that level 7 cannot be reached with 64 kB data blocks as the maximum size of a
file using 64-bit disk addresses is reached at level 6 (the file size is limted by
the size of 64-bit numbers). Most operating systems will fail at file sizes a lot
smaller than these; disks of this size do not exist (yet).

To keep some perspective, as I write this in February 2015, standard drives are 1-4 TB
in size. Drives like this have sequential sustained transfer rate for large transfers
of order 100 MB/sec, so in the best of all possible worlds, to copy a 1 TB file from
1 drive to another (so no head seeking), would take at least 2000 seconds. Actual 
random access times for small transfers are much slower due to the disk access
times. If you can afford a large solid state drive, you will get better performance,
and vastly better random access times for small transfers.

\subsection DiskLookupRead Finding data quickly in a file
The point of having the lookup tree is to speed up random access to data in the file.
The time to locate data is determined by the number of disk reads that are required
to load the data.

### How the Son32 system looks up data ###
The old \ref Son32 system has channel data blocks that form a doubly-linked list.
That is, each block holds the disk address of the previous and next block. This makes
sequential access very fast, but random access very slow. To speed up random access,
there is an optional table of up to 2048 pointers into the data file. These are
approximately equally spaced through the data.

In a Son32 file, the average number of reads to locate data is proportional to the
channel size. A channel with 4 GB of data has around 60 data blocks between each
position in the lookup table. In this case we can usually locate data in an
average of 15 or so reads (can be more if the data is not evenly spaced in time).

### How the Son64 system looks up data ###
In a \ref Son64 file, the number of reads to locate the data is proportional to the
logarithm of the channel size. A 4 GB file will have 2 levels of index,
with a level 2 index in memory. It takes 1 read to get the appropriate level 1
index and 1 read to get the data. It can take extra reads if we need to scan forward
from the block the lookup locates if the search time ilies between the last time in a
block and the start of the next.

### How the Son32 and Son64 systems compare when looking up data ###
Assuming that the Son32 file contains a lookup table, both systems are the same
(requiring no extra reads) for channel data sizes up to 16.6 MB. The Son64 system
requires 1 extra read compared to Son32 for channel sizes from 16.7 to 66 MB.
From 67 to 133 MB both require 1 extra read. From a channel size of 134 MB onwards,
the Son32 system needs more extra reads. 

However, block sizes in the Son64 are double the size of the Son32 system, which
tends to mitigate the extra reads for small channel sizes. We have not noticed any
speed differences for small files; as file sizes increase the Son64 system becomes
faster.

\section fileBlockData Data blocks
The data blocks saved on disk are of type TDataBlock. This is currently declared as:

\code
#include "s64dblk.h"
struct TDataBlock : public TDiskBlockHead
{
    union
    {
        TSTime64 m_event[MAX_EVENT];    //!< Events, set to fill the entire block
        TMarker  m_mark[MAX_MARK];      //!< Markers, set to fill the entre block
        TExtMark m_eMark[1];            //!< Start of first item; size known at run time
        TWave<short> m_adc;				//!< Start of first item; varying size
        TWave<float> m_realwave;		//!< Start of first item; varying size
    };
};
\endcode

The TDataBlock type starts with a TDiskBlockHead followed by repeated data objects that
depend on the channel type. Event and marker channels are easy to understand as it is
possible to specify the layout in terms of C++ structures. However, waveforms and
extended markers cannot be specified this way as the sizes of the repeated objects
that hold data are not constant.

However, it should be noticed that the very first data item after the TDiskBlockHead is
always the time of the first data item in the TDataBlock.

\subsection DataBlockEvent Event channel data
Event channels are a list of TSTime64 (64-bit integer) times, in ascending time order.
The maximum number that can fit in a data block is held in the MAX_EVENT constant
which is 8190 with the standard settings.

\subsection DataBlockMarker Marker channel data
Marker channels a list of TMarker items. Each TMarker starts with a TSTime64 item. The
maximum number of items in a TDataBlock is given by the MAX_MARK constant, which is
4095 with the standard settings.

\subsection DataBlockExtMark Extended marker channel data
These channel types (TextMark, RealMark and AdcMark) are a TMarker with attached data.
The size of the attached data is fixed for each channel, but is user-defined when the
data channel is created. This means that the data items cannot be expressed as a
simple array as is done for event and marker data. Instead, code that handles this
data either has to explicitly move pointers between data items by casting to `char*`,
adding the increment to the next item (always a multiple of 8 bytes), then casting back
to the correct type. This can be simplified by making use of the special iterators
defined for this purpose in s64iter.h and used as the `db_iter<t, size>` type.

There is no constant defined for the maximum number of extended markers as it
depends on the size of each marker.

\subsection DataBlockWave Waveform data blocks
To allow us to deal with waveform channels with gaps, we do not deal with arrays of
short (for Adc channels) and arrays of float (for RealWave) channels directly. Instead
the data block holds a sequence of TWave<short> or TWave<float> objects that encapsulte
a continuous steam of equally spaced (in time) data. These objects are not of a fixed
size. They are composed of a 16 byte header that holds the time of the first waveform
item and the number of waveform items that follow the header.

The waveform items are then an array of data of the appropriate type. The space used
by the waveform data is always rounded up to a multiple of 8 bytes.

To make it easier to deal with waveform TDataBlock objects, the file s64witer.h holds
definitions of a waveform iterator class that iterate over TWave<T> objects. Interesting
as this is, unless you need to fix a bug in the library, you should never need to know
anything about the TWave<short> or TWave<float> objects.
*/

//-------------------------------------------------------------------------
/*!
\page LibInt Library interface
\tableofcontents
The CSon64File class defines a pure virtual interface to a concept of a file system. It
defines the set of functions that you use to create and open files and read and write
thier contents. Of itself, it says nothing about how the data is stored.

We have implemented two 'concrete' interfaces:
- TSon64File to interface to the new 64-bit library format (s64priv.h)
- TSon32File to interface to the old 32-bit library format (s32priv.h)

If you wanted, you could implement interfaces to other file types using the same
interface. To do this, inherit the CSon64File interface and override all the
routines. Ideally, the only public interface difference should be the class constructor.
This would allow you to use the other file types exactly as if they were a
64-bit Son library file. You should study the declarations of the TSon64File and the
TSon32File classes for examples. In particular, note the warning at the start of the
TSon64File class regarding declaring the entire class DllClass.

The following code shows how you could open an existing file which can be either a Son32 or a
Son64 file. Note the use of a pointer to a CSon64File to deal with generic files rather than
explicity dealing with a TSon32File or a TSon64File. This is basically what Spike2 does, though
if opening as a 32-bit file fails, it also tries to open as a 64-bit file.

\code
#include "s64priv.h"
#include "s32priv.h"
using namespace ceds64;
unique_ptr<CSon64File> file;
std::string fileName = GetFileName();    // Some code to get a suitable name
bool bOpenNew = IsSon64Name(fileName);   // Some code to decide type based on name
file.reset(bOpenNew ? static_cast<CSon64File*>(new TSon64File) : new TSon32File);
int iErr = file->Open(fileName.c_str()); // iErr will be S64_OK (0) if it opened
\endcode

\note In the `file.reset()` call, we need the cast to CSon64File* because the type of the
result of the ternary operator is determined by the types of the second and third arguments.
Without the `static_cast<CSon64File*>`, this will not compile because a TSon32File* cannot
be converted to a TSon64File* or vice versa. However, both can be converted to CSon64File*,
so if we cast either to this, the other will also be converted to this type.

If you are creating a new TSon32File for writing, you can generate a big format file (allowing up
to 1 TB of data rather than 2 GB) by using `new TSon32File(1)` in place of `new TSon32File`.

Note that you cannot create instances of CSon64File, only of the concrete file types. In the
code examples that follow we assume that you are opening a 64-bit file and we do not open the
file through a pointer to make the examples simpler. We expect that in real use you would
follow the pattern above using a unique_ptr. See CSon64File for the documentation of all the routines.

\section namespace The ceds64 and ceds32 namespaces
All the interface elements defined in the s64.h header file are in the ceds64 namespace. You wil need
to set a `using namespace ceds64;` at the start of your cpp modules that include the headers, or you
will need to qualify all the names from the library.

Inside the TSon32File implementation, we include the old 32-bit library code. We wrap all this in the
ceds32 namespace to avoid name clashes. In the most unlikely event that you need to include the old son.h header (which you should
not need to do), we suggest you do something like:

\code
namespace ceds32
{
#include "son.h"
}

ceds32::TSTime t32_bit;
\endcode

This will avoid name clashes with types declared in s64.h. You should not need to do this, as
you can read and write Son32 files using the new interface.

\section unicode Strings and Unicode
All strings stored inside the library are handled as std::string with UTF-8 encoding.
The library does not do anything with these strings, other than copy them, so saying that all
strings are UTF-8 is more of a specification for the user than the library. There are
issues if strings get truncated, where we try to keep to Unicode character boundaries.

The ASCII character set for character codes 0 to 0x7f (127) is also UTF-8 compatible.
Users of the old \ref Son32 format files are advised to stick to ASCII characters.
This is because Son32 text fields are of defined (and quite short) lengths. For example,
the channel Units are limited to 5 8-bit characters. As Unicode characters codes greater
than 0x7f encode as 2 or more UTF-8 bytes, using other characters can cause unexpected
character truncations. This is not a problem for Son64 files.

The library must interface with the operating system to open and create files.

In Windows, the underlying OS uses 16-bit wide characters as the native format.
If you build the library for use with Unicode (which defines \_UNICODE) we provide both
wchar_t and char type versions of the CSon64File::Open() and CSon64File::Create() calls.
At CED we _always_ build in this configuration. This saves the caller
converting a string from UTF-16 (the Windows standard) to UTF-8, then for the library
to convert it back to UTF-16 to call into the operating system to open the file.

In modern desktop Linux, the assumption is that everything is UTF-8, so we do not provide
extra wide character versions of Open() and Create().

Before Spike2 versiom 8.03, all text was encoded with the Microsoft DBCS system. As long
as users stuck to ASCII characters all was well, but old files may also contain national
characters with Unicode interpretations that depend on the current code page. Spike2
attempts to work around this when it reads text by checking for non-UTF-8 compatible
sequences. If any are found, the text is converted to Unicode on the assumption that it
is coded for the current code page.

\section readstrings Reading strings
The original design of the library interface used std::string to pass strings into and
out of the library. This was really very convenient, especially when reading strings.
However, it caused all sorts of problems when the library was built as a DLL. For
simplicity, all routines that transfer text into and out of the library use simple
char* strings. This is usually not a problem when writing a string, but can be
troublesome when reading them as the length of the string is unknown.

All the routines that return a string (GetFileComment(), GetChanComment(), GetChanTitle()
and GetChanUnits()) will return the length without returning the string. You can use this
to allocate space. Here is an example for GetFileComment():

\code
#include "s64priv.h"
using namespace ceds64;
TSon64File file;
int iErr = file.Open(fileName);    // Simplest possible file open call

vector<char> vStr;
int nNeeded = file.GetFileComment(0);   // Space needed for the first comment
if (nNeeded > 0)                    // You should get 1 for an empty string
{
    vStr.resize(nNeeded);           // make space for text and 0 terminator
    file.GetFileComment(0, nNeeded, vStr);
    printf("Comment: ", vStr.data());
}
\endcode

If you find this tedious, you can easily write wrappers for the functions that does
this for you and returns the result as a string of your choice:

\code
int GetFileComment(CSon64File* pF, std::string& s, int n)
{
    s.clear();
    int nNeeded = pF->GetFileComment(n);
    if (nNeeded > 0)
    {
        vector<char> vStr(nNeeded);
        pF->GetFileComment(n, nNeeded, vStr.data());
        s = vStr.data();
    }
    return nNeeded;
}
\endcode


\section Compiler C++ Compiler
The library generates data objects with all public members implemented as virtual functions.
The release build of Spike2 version 8.04 and the Son64 DLL are currently built with the
Visual Studio 2012 C++ toolset that supports Windows XP (v110_xp) with the library set to
Multithreaded DLL in Release mode with the Unicode character set. We will move on to a more
recent version of the compiler in due course.

We build both a 64-bit (x64) version and a 32-bit (Win32) version of the DLL for use by the
x64 and Win32 versions of Spike2. We anticipate dropping the x86 builds once there is no
demand for them.

If you want to use the DLL that is distributed with Spike2 version 8, you will need to
use a compiler that is compatible with this. This is an issue because all the functions
in the library are virtual, which means that the calling code must match the Microsoft
specifications. The interface to the library is defined in terms of primitive types and
all memory buffers used to pass or return information are allocated and disposed of by
the caller. This should minimise interface problems.

If you are using a different compiler system and want to use the DLL interface (maybe you
have a lot of utility programs and want to keep them all small) you may need to build the
DLL yourself with your own compiler to allow you to access the CSon64File objects
correctly. Of course, if you link the library code into your application, there will
be no problems.

We remind you that in all cases you must respect the \ref Licence "software licence."

\section operatingSystems Operating systems
The library has been extensively used in Windows where it has been build in both x86
(32-bit) and x64 (64-bit) code. The Windows build targets a DLL, though it would build
just as well as a static library. It was built in Windows using the Visual Studio 2012
compiler.

The __S64_OS__ macro in s64.h sets the operating system in use. Current values are:

- __1__ For Windows
- __2__ For Linux

We will add further values as they are needed.

There are two macros that are very important when building and using the Son64 code.
They are used to determine what the macro DllClass is defined as. The DllClass macro
is applied to every object in the library that is exported for external use.

- __S64_NOTDLL__ If this is defined, the code is being built either as a library or
                 as part of the application. If it is not defined, the code is being
                 built as a Windows DLL or equivalent in other operating systems.
- __DLL_SON64__  If this is defined and S64_NOTDLL is not defined, we are building
                 the DLL. If this is not defined and S64_DLL is not defined, then
                 we are including the headers in our code to link to the Son64 DLL.

When building as a DLL in Windows, we define DLL_SON64 as a predefined symbol and do
not define S64_NOTDLL. This causes the DllClass macro to be defined as
`__declspec(dllexport)` in the header files, which in turn is applied to all 
routines exported from the DLL. When using the DLL, do _NOT_ define DLL_SON64 or
S64_NOTDLL. This causes DllClass to be defined as `__declspec(dllimport)`
which marks all the external files declared in the header as being found in a DLL.
Any header that defines DllClass also undefines it at the end and does NOT include any
other header between the definition and the \#undef DllClass.

The same system could be used in LINUX, if desired. Currently, DllClass is defined as
nothing in LINUX.

There are conditionals in the s64*.* source code to build under LINUX, but these have
not been tested. The S64_OS macro is defined as S64_OS_WINDOWS or S64_OS_LINUX. The
macro is used to determine how files open, close, seek and buffer flushing are done
and which set of include files is used.

The 32-bit son library code that is included in the Son64 library attemps to detect
linux itself. This code has been seen to build and run in Linux.

\def S64_NOTDLL
If you define this before including any of the s64*.h files, we assume that you will
be building the Son64 library as part of your application, or using it as a library
in some way. Any setting of #DLL_SON64 is ignored. This causes the #DllClass macro
to be defined as nothing. If you do not define this, we assume that you are either
building (if DLL_SON64 is defined) or using (if DLL_SON64 is not defined) the library
as a DLL.

\def DLL_SON64
This is ignored if #S64_NOTDLL is defined. Otherwise, if defined it means that we are
building the library as a DLL. In Windows this means we define DllClass as
'__declspec(dllexport)`. If it is not defined, we are using the header files only to
link to the DLL and DllClass is defined as `__declspec(dllimport)`. This is not
implemented for LINUX use, but could be.

*/

//------------------------------------------------------------------
/*!
\page ReadFile Reading data using the Son64 library
\tableofcontents
In these examples we assume that we are working with a 64-bit file, but they would work for a 32-bit file
if you changed the file type from TSon64File to TSon32File and included s32priv.h in place of s64priv.h.
If you want to have the option of opening either a Son64 or a Son32 file, see \ref LibInt for
details, then access the file though a pointer (so `file.Open()` becomes `file->Open()`,
for example).

\section OpenExisting Opening an existing file
Before we can read any data we must create a suitable file object and then open the file:

\code
#include "s64priv.h"
using namespace ceds64;
TSon64File file;
int iErr = file.Open(fileName);    // Simplest possible file open call
\endcode

The exampled create file objects on the stack for simplicity. If you use a naked `new` to create
a file object you must remember to delete it:
\code
CSon64File pFile = new TSon64File;
...             // use the file
delete pFile;   // You MUST remember to do this
\endcode

The preferred modern C++ usage is to use a `std::unique_ptr` to ensure that the objects
get detroyed when they go out of scope.
\code
unique_ptr<CSon64File> pFile( new TSon64File );
...             // use the file. It is deleted automatically...
                // ...when pFile goes out of scope
\endcode

The open routine returns #S64_OK (0) if all was well or a #S64_ERROR error code. If
the file opens without error, you can then use the file object to access the file.

\section ReadFileInfo Reading file information
In many cases you will know exactly what information is held in the data file, but if you
don't know, there are routines to obtain it. The following example prints out basic file
information and the type, channel divide and title and comment for every channel in the
file (we assume that `fileName` holds a suitable path to the file).

\code
using namespace ceds64;
TSon64File file;
file.Open(fileName);                    // Open a file, assume no error

double dTimeBase = file.GetTimeBase();  // Get the time base in seconds
printf("Time base in us: %8g\n", dTimeBase*1e6);

// Example of reading and allocating space for whatever size is needed
int nSpace = file.GetFileComment(0);    // get space for first comment
std::vector<char> work(nSpace);         // allocate sufficient space
file.GetFileComment(0, nSpace, work.data());
std::string s(work.data());             // comment now in the string
printf("Comment 0: %s\n", s.c_str());

// Read any further comments with a fixed-size buffer
for (int c=1; i<NUMFILECOMMENTS; ++c)
{
    char szComment[100];
    file.GetFileComment(c, sizeof(szComment), szComment); // assume no error
    if (szComment[0])
        printf("Comment %d: %s\n", c, szComment);
}

const int nMaxChans = file.MaxChans();  // get the channel count
for (TChanNum i=0; i<nMaxChans; ++i)    // iterate over the channels
{
    TDataKind kind = file.ChanKind(i);  // get the channel type
    switch (kind)
    {
    case ChanOff:   break;	        // channel unused
    case Adc:       printf("%2d is Adc data\n", i); break;
    case EventFall: printf("%2d Event on falling edge\n", i); break;
    case EventRise: printf("%2d Event on rising edge\n", i); break;
    case EventBoth: printf("%2d Event on both edges\n", i); break;
    case Marker:    printf("%2d Marker data\n", i); break;
    case AdcMark:   printf("%2d AdcMark data\n", i); break;
    case RealMark:  printf("%2d RealMark data\n", i); break;
    case TextMark:  printf("%2d TextMark data\n", i); break;
    case RealWave:  printf("%2d RealWave data\n", i); break;
    default:        printf("Error: %2d is unknown type\n");
    }

    if (kind != ChanOff)            // if the channel is used...
    {
        char szComment[100];        // Arbitrary space for a comment
        char szTitle[40];           // Arbitrary space for a title
        file.GetChanComment(i, sizeof(szComment), szComment);
        file.GetChanTitle(i, sizeof(szTitle), szTitle);
        printf("Title: %s, Comment: %s\n", szTitle, szComment);
    }

    // For waveform and related channels, calculate the sample interval
    if ((kind == Adc) || (kind == RealWave) || (kind == AdcMark))
    {
        TSTime64 tDivide = file.ChanDivide(i);  // Get channel divide
        printf("Sample interval: %g seconds\n", tDivide * dTimeBase);

        char szUnits[20];       // Arbitrary units space
        file.GetChanUnits(i, sizeof(szUnits), szUnits);

        double dScale, dOffset;
        file.GetChanScale(i, dScale);
        file.GetChanOffset(i, dOffset);
        printf("Scale: %f, Offset: %g, Units: %s\n", dScale, dOffset, szUnits);
    }

    // Show more for extended markers
    if ((kind == TextMark) || (kind == RealMark) || (kind == AdcMark))
    {
        size_t nRows, nCols;
        int nPre = file.GetExtMarkInfo(i, &nRows, &nCols);
        printf("Rows: %d, Cols: %d\n", (int)nRows, (int)nCols);

        if (kind == AdcMark)
            printf("Pre-trigger points: %d", nPre);
    }
}
\endcode

\section TimeRange Time ranges
The \ref Son64 file system uses time ranges from a start time (often documented as `tFrom`)
that run _up to but not including_ an end time (often documented as `tUpto`). This is
different from the \ref Son32 system which used time ranges that ran from a time
_up to and including_ an end time.

If you are translating a program that used the old 32-bit library through the interface
defined by son.h, you may need to increase the end times by 1 clock tick. The TSon32File
interface (which uses the son.h interface behind the scenes) deals with these differences 
automatically, so time ranges in TSon32File and TSon64File behave in the same way.

When you specify a time range for an operation, the `tFrom` time must be less than the
`tUpto` time, or the time range is empty and the operation will find no data.

\section ReadEventChanData Reading event channel data
CSon64File::ReadEvents() is used to collect Event, Marker, AdcMark, RealMark and TextMark
times from the file and place them in a TSTime64 array. If you want to read marker times
and marker data you must use the CSon64File::ReadMarkers() function and you must use
CSon64File::ReadExtMarks() to get at AdcMark waveforms, RealMark real data or TextMark
characters.

For this example, we will assume that channel 0 holds events (or any marker-derived type).
We want to get up to 100 event times from the start of the file (time 0) up to
_but not including_ 100,000 clock ticks. Use a call of the form:

\code
using namespace ceds64;
TSon64File file;
file.Open(fileName);                        // Open a file, assume no error
double dTimeBase = file.GetTimeBase();      // get the file time base
TSTime64 aTimes[100];                       // space for the times
int nGot = file.ReadEvents(0, aTimes, 100, 0, 100000);
for (int i=0; i<nGot; ++i)
    printf("%f\n", aTimes[i]*dTimeBase);    // display the times in seconds
\endcode

CSon64File::ReadEvents() returns the number of events copied to the buffer or a negative
error code. To cope with reading through a lot of event data you will probably want to
use a reasonably sized buffer and iterate through blocks of events.

\code
using namespace ceds64;
TSon64File file;
file.Open(fileName);                        // Open a file, assume no error
const int nBufSz = 100;                     // the buffer size
TSTime64 aTimes[nBufSz];                    // space for the times
TSTime64 tFrom = 0;                         // Start of the time range
TSTime64 tUpto = 1000000;                   // some time limit
int nGot;
do
{
    nGot = file.ReadEvents(0, aTimes, nBufSz, tFrom, tUpto);
    if (nGot > 0)
    {
        ProcessEvents(aTimes, nGot);        // Your code to process nGot events
        tFrom = aTimes[nGot-1] + 1;         // next unique event time
    }
}while( (nGot == nBufSz) && (tFrom < tUpto) );
\endcode

This will only go around the loop again if the previous read filled the buffer and the
time range is not exhausted. This also demonstrates why we do not want multiple events
at the same time in a data channel as it becomes difficult to separate them if they
happen to fall at the end of a read buffer. You can also see that we depend on events
being written with monotonically increasing times.

\section ReadWaveChan Reading a waveform channel
CSon64File::ReadWave() reads contiguous Adc or RealWave data between two times from an
Adc, AdcMark or RealWave channel. There are two overrides of the function to read into
16-bit integers (short) or into 32-bit floating point (float) data. If you read from a
RealWave channel into an array of shorts, or from an Adc channel into an array of float,
the library uses the scale and offsets set for the channel to convert the data. See the
description of \ref Waveform for details.

Both functions return the number of contiguous points found from the starting time and
they also return the time of the first waveform point. If your waveform data is fragmented
(i.e. there are gaps in the recording) you will need to call the function at least once
for each fragment. The first point returned is timed at or after the start time of the
search. The search extends up to, _but not including_ the end time.

You could collect waveform data from time 0 up to 1000000 clocks ticks into data arrays
of 100 elements from channel 1 (Adc, RealWave or AdcMark data) using:

\code
using namespace ceds64;
TSon64File file;
file.Open(fileName);                        // Open a file, assume no error
TSTime64 tDivide = file.ChanDivide(1);      // get the channel divide
const int nBufSz = 100;                     // the buffer size
float aData[nBufSz];                        // space for the data
TSTime64 tFrom = 0;                         // Start of the time range
TSTime64 tUpto = 1000000;                   // some time limit
TSTime64 tFirst;                            // set to time of first data
int nGot;
do
{
    nGot = file.ReadWave(1, aData, nBufSz, tFrom, tUpto, tFirst);
    if (nGot > 0)
    {
        ProcessWave(aData, nGot, tFirst);   // process the data
        tFrom = tFirst + nGot * tDivide;    // next time to read
    }
} while((nGot > 0) && (tFrom < tUpto));
\endcode

If you select an AdcMark channel and supply a pointer to a CSFilter object, only AdcMark
events that are accepted by the filter contribute to the result. Reads from an AdcMark
channel can return waveforms from multiple events as long as there are no gaps in the
waveforms.

If you select an AdcMark channel with multiple traces, you can choose which trace to 
return by use of the CSFilter::SetColumn() command with a filter.

\section ReadMarkerChan Reading Marker data
CSon64File::ReadMarkers() reads both the marker times and the marker bytes attached to
each marker from a Marker, AdcMark, RealMark or TextMark channel. If you only need the
marker times you can use CSon64File::ReadEvents().

Marker data can be filtered and this example will set a marker filter so that we only
accept data with a first marker code of 1.

If we assume that channel 2 holds Marker (or AdcMark, RealMark or TextMark) data then
we can read and process Marker data with:

\code
#include "s64filt.h"
using namespace ceds64;
TSon64File file;
file.Open(fileName);        // Assume opens without error
CSFilter filter;            // Make a filter that accepts everything
filter.Control(0, -1, CSFilter::eS_clr);    // clear all layer 0
filter.Control(0, 1, CSFilter::eS_set);     // allow layer 0, code 1

const int nBufSz = 100;     // buffer size used for reading
TMarker aData[nBufSz];      // work space
TSTime64 tFrom = 0;         // start of read
TSTime64 tUpto = 1000000;   // Where to read up to
int nGot;
do
{
    nGot = file.ReadMarkers(2, aData, nBufSz, tFrom, tUpto, &filter);
    if (nGot > 0)
    {
        ProcessMarkers(aData, nGot);        // Process the data
        tFrom = aData[nGot-1].m_time + 1;   // next unique data time to read
    }
} while ((nGot == nBufSz) && (tFrom < tUpto));
\endcode

\section ReadAdcMarkData Reading AdcMark data
Reading AdcMark data is trickier than reading any of the above data types because the
number of waveform data points (although fixed for a particular channel) is variable. This
means that the structure used to hold AdcMark data in memory has to be allocated dynamically,
and the programmer is responsible for moving pointers forwards and backwards as there
is not built-in C++ construct to do this for you.

You can find out the number of points of waveform information attached to the AdcMark using
CSon64File::GetExtMarkInfo(). The size, in bytes, of each AdcMark item in the channel is
returned by CSon64File::ItemSize().

To make a little easier, you can use the db_iterator template class defined in
s64iter.h, to generate an iterator that hides the pointer arithmetic. The following will
let you process AdcMark data. For simplicity, the example does not filter the data. We
assume that channel 3 holds AdcMark data:

\code
#include "s64iter.h"
using namespace ceds64;

// This stands for your code to process the data.
// It returns the time of the data item
TSTime64 ProcessAdcMark(const TAdcMark& am, int nPtperTrace, int nTrace)
{
    // am.m_code[n] gives access to the codes
    // am.Shorts() returns a pointer to the interleaved short data. If there
    // were 4 interleaved traces, the data is organised as:
    // pt0 trace0, pt0 trace1, pt0 trace2, pt0 trace3, pt1 trace0, pt1 trace2 etc.
    return am.m_time;
}

TSon64File file;
file.Open(fileName);                    // assume all opens OK

// Get information, then allocate a memory buffer for 100 items
size_t nPtPerTrace, nTraces;            // space for the points per trace and traces
int nPre = file.GetExtMarkInfo(3, &nPtPerTrace, &nTraces); // nPre is negative for errors
int nBytePerItem = file.ItemSize(3);    // Bytes needed per item
const int nBufSz = 100;                 // Items in the buffer
std::vector<char> vWork(nBufSz*nBytePerItem);   // buffer space needed
TAdcMark* pAM = reinterpret_cast<TAdcMark*>(vWork.data()); // points at start of buffer

TSTime64 tFrom = 0;                     // start of range to process
TSTime64 tUpto = 1000000;               // end of time range to process
int nGot;
do
{
    nGot = file.ReadExtMarks(3, pAM, nBufSz, tFrom, tUpto);  // simple read, no filter
    if (nGot > 0)
    {
        db_iterator<TAdcMark> it(pAM, nBytesPerItem);   // object to iterate the data
        TSTime tLast;
        for (int i=0; i<nGot; ++i, ++it)
            tLast = ProcessAdcMark(*it, nPtPerTrace, nTraces);
        tFrom = tLast + 1;          // next time to read
    }
} while ((nGot == nBufSz) && (tFrom<tUpto));
\endcode

There is no need to use db_iterator to do the pointer arithmetic, but using it it avoids
code of the type:
\code
TAdcMark* pAM = some pointer;
...
pAM = reinterpret_cast<TAdcMark*>(reinterpret_cast<char*>(pAM) + nBytePerItem);
\endcode

\warning If you do use db_iterator and you have an iterator object it, you cannot copy data
using the iterator as *it. This may appear to work, but `sizeof(TAdcMark)` is not the same
as the value returned by CSon64File::ItemSize() you will obtain the TMarker part of the data
and maybe 2 or 4 waveform points, but the object will be truncated. You must copy variable
size objects with `memcpy()` or `memmove()` as appropriate, using the correct item size. You
can use ++it and it++ and it[n] to manipulate pointers, but not to copy data.

\section ReadRealMarkData Reading RealMark data
Reading RealMark data is very similar to reading AdcMark data in that the number of real
numbers associated with the data type (although fixed for a particular channel) is variable.
This means that the structure used to hold RealMark data in memory must also be allocated
dynamically, and the programmer has to be responsible for moving pointers forwards and
backwards. You can find out the number of real numbers attached to the RealMark using 
CSon64File::GetExtMarkInfo(). The size, in bytes, of the complete RealMark is returned by
CSon64File::ItemSize(). Currently, all software generates RealMark data with one column,
but the library can handle multiple columns, just as it does for AdcMark data.

If you are to read RealMark data from channel 4 you will need code like:
\code
#include "s64iter.h"
using namespace ceds64;

TSTime64 ProcessRealMark(const TRealMark& rm, int nRows, int nCols = 1)
{
    // rm.m_code[n] gives access to the codes
    // rm.Floats() returns a pointer to the data.
    return rm.m_time;                   // return the item time
}

TSon64File file;
file.Open(fileName);                    // assume all opens OK
size_t nRows, nCols;                    // space for the points per trace and traces
file.GetExtMarkInfo(4, &nRows, &nCols); // nCols is usually 1
int nBytePerItem = file.ItemSize(4);    // Bytes needed per item
const int nBufSz = 100;                 // Items in the buffer
std::vector<char> vWork(nBufSz*nBytePerItem);   // buffer space needed
TRealMark* pRM = reinterpret_cast<TRealMark*>(vWork.data()); // points at start of buffer

TSTime64 tFrom = 0;                     // start of range to process
TSTime64 tUpto = 1000000;               // end of time range to process
int nGot;
do
{
    nGot = file.ReadExtMarks(4, pRM, nBufSz, tFrom, tUpto);  // simple read, no filter
    if (nGot > 0)
    {
        db_iterator<TRealMark> it(pRM, nBytesPerItem);   // object to iterate the data
        TSTime64 tLast;
        for (int i=0; i<nGot; ++i, ++it)
            tLast = ProcessRealMark(*it, nPtPerTrace, nTraces);
        tFrom = tLast + 1;              // next time to read
    }
} while ((nGot == nBufSz) && (tFrom<tUpto));
\endcode

\section ReadTextMarkData Reading TextMark data
Reading TextMark data is very similar to reading both AdcMark and RealMark data. The only
real difference being in the extraction of the text. If you are to read TextMark data
from channel 5 you will need code of the form:
\code
#include "s64iter.h"
using namespace ceds64;

TSTime64 ProcessTextMark(const TTextMark& tm)
{
    // tm.m_code[n] gives access to the codes
    printf("Code 0: %d, TextMark text: %s\n", tm.m_code[0], tm.Chars());
    return tm.m_time;                   // return the item time
}

TSon64File file;
file.Open(fileName);                    // assume all opens OK
size_t nChars;                          // space for the points per trace and traces
file.GetExtMarkInfo(5, &nChars);        // Only 1 column for TextMark
int nBytePerItem = file.ItemSize(5);    // Bytes needed per item
const int nBufSz = 100;                 // Items in the buffer
std::vector<char> vWork(nBufSz*nBytePerItem);   // buffer space needed
TTextMark* pTM = reinterpret_cast<TTextMark*>(vWork.data()); // points at start of buffer

TSTime64 tFrom = 0;                     // start of range to process
TSTime64 tUpto = 1000000;               // end of time range to process
int nGot;
do
{
    nGot = file.ReadExtMarks(5, pTM, nBufSz, tFrom, tUpto); // simple read, no filter
    if (nGot > 0)
    {
        db_iterator<TTextMark> it(pRM, nBytesPerItem);   // object to iterate the data
        TSTime64 tLast;
        for (int i=0; i<nGot; ++i, ++it)
            tLast = ProcessTextMark(*it);
        tFrom = tLast + 1;              // next time to read
    }
} while ((nGot == nBufSz) && (tFrom<tUpto));
\endcode

\section CloseFile Closing the file
You can close the file associated with a CSon64File object using the CSon64File::Close() command.
If you do not call Close(), it is called for you when the CSonFile64 object is destructed. At the
time of writing, CSon64File objects are not designed to be reused for multiple files (a sequence of
Open(), use, Close(), Open(), use, Close()...), though they may well survive the experience. 
*/

//-------------------------------------------------------------------
/*!
\page WriteData Writing a new file
\tableofcontents
Creating files is a more complicated process than reading an existing file as you must specify
all the file properties. This requires you to know and understand rather more about the file
than when reading (where the file creator has done all this for you).

It is also possible to write data to a file that was opened for reading (unless the file was
opened in Read Only mode). There are also data editing oportunities for waveform data and
by using CSon64File::EditMarker(). You can also delete channels and reuse them for more
drastic changes. If you delete a channel and reuse it, the previous disk space of the channel
is reused until it is exhausted before more disk space is allocated. Reusued channels are
marked as such by incrementing a channel ID value in the channel header on disk.

\section CreateFile Creating a new file
To create a new data file you need to first create a file object. If you want to have a
choice of creating a Son32 or a Son64 file, you should open the file object using a pointer
as described in \ref LibInt. In the examples that follow we will assume that we are dealing
with a TSon64File and we will use the simplest posssible code.

When you create a new file you must configure it by setting the maximum number of channels
it can contain (which determines how much file header space to allocate to the channels),
and how much user space you want to allocate (usually none). Currently user space is limited
to a maximum of 65536 bytes of space (MAXHEADUSER), but could be increased if desired. If
you allocate user space it is accessed by CSon64File::SetExtraData() and
CSon64File::GetExtraData() and
CSon64File::GetExtraDataSize(). The contents of user-space are entirely up to the user; the
library never looks at it. If you set user-data, you probably also should use the
CSon64File::AppID() command to identify the program that generated the data as a clue to
what the user data area might contain.

Simple example of creating a new data file and setting the file creator:
\code
#include "s64priv.h"                // include for TSon64File, use s32priv.h for TSon32File
using namespace ceds64;
std::string fName = GetFilePath();  // Get the file name
const int nChans = 64;              // Maximum channels in the file
TSon64File file;                    // A file object
int iErr = file.Create(fName.c_str(), nChans);  // Create file, no user space
if (iErr)
    ...                             // deal with failure

// Optional: Set a creator
TCreator creator;                   // This sets creator string to 0s
const char* pMyApp = "MyApp01";     // Some identifier
memcpy(creator.acID, pMyApp, std::min(strlen(pMyApp), sizeof(creator.acID)));
file.AppID(nullptr, &creator);
\endcode

\section SetTimeBase Set file time resolution (the time base)
The basic concept of the time base is explained in \ref ClockTick.
Every data item in a Son64 or Son32 file has a time, and the times are set in integer
multiples of the file time base

If you have specific waveform sampling rates to achieve, you need to choose a
timebase that allows you to achieve the sample rates you want (such that all the sample
intervals of the waveforms are integer multiples of the file time base). If this interval
is not short enough, just divide it by an integer value until it is has the resolution you
need.

If the raw data you are saving is derived from a hardware system, this system may well
decide the time resolution that is available to you. You do have the option of setting the
file time base to be an integer fraction of this so as to allow you to position derived data
between the times that can be achieved by the hardware.

If you do not set a time base in a new file, it will default to 1 microsecond.

As an example, if you want to store waveforms that were sampled at 100, 300, 400 and 500 Hz
you need to find a time resolution that allows you to generate all these frequencies. The
lowest common multiple of these rates is 6000 Hz, so the longest time base you could use is
1/6000 of a second (166.666... microseconds). If you wanted a time resolution of nearer to
a microsecond you could set the time base to 1.0/(6000*167) (approximately 0.998004 us).
The code to set this looks like:
\code
#include "s64priv.h"
using namespace ceds64;
TSon64File file;                // A file object
file.Create(fName, 50);         // Create a file with 50 channels, assume no error
file.SetTimeBase(1.0/(6000.0 * 167);    // Set the time base
\endcode

\section FileComments File comments and sample start time
You can set up to NUMFILECOMMENTS (currently 8) in a Son64 file (the limit is 5 for a Son32
file). All strings stored in a Son64 file are coded as UTF-8 and there is no limit on the
string length, other than common sense and the fact that all the strings must fit in the
file header. See \ref StringTable for more information.

You can set a date and time to match the start sampling instant in the file, should you
choose. Spike2 sets the start sampling time and date as accurately as it can and has an
x axis display mode that shows time of day for data files, which is based on the saved
time and date (not the file system time and date of the file).

\code
#include "s64priv.h"
using namespace ceds64;
TSon64File file;                // A file object
file.Create(fName, 32);         // Create a file, assume no error

file.SetFileComment(0, "The first file comment");
std::string comment = GetAComment();     // collect a user comment from somewhere
file.SetFileComment(1, comment.c_str()); // and set it

TTimeDate td;                   // Date and time to 1/100th of aecond
td.clear();                     // sets all fields to 0
td.wYear = 2015; td.ucMon = 1; td.ucDay = 20;   // Set a date
td.ucHour = 15; td.ucMin = 50; td.ucSec = 59;   // and a time
file.TimeDate(nullptr, &td);
\endcode

\section DefineChans Define the channels
Channels are identified by a channel number, an integer in the range 0 to 
CSon64File::MaxChans()-1. The maximum number of channels in the file is set when the
file is created by CSon64File::Create(). If you are creating channels in a file that
already holds data channels you can get the lowest unused channel number with
CSon64File::GetFreeChan().

If you run out of channels, currently you will need to create a new file with more
channel space and copy the old data into it. However, the design of the Son64 system
would allow us to add more channels to an existing file (but this is not yet supported
by the API).

\subsection ChanRate Channel dRate parameter
All event-based channels have a dRate parameter. This is used internally by the library
when allocating space for circular buffering. You should set this to the expected maximum
sustained event rate (in Hz) over a period of several seconds. See \ref PgWrBuf for more
about circular buffering. It is likely that software that reads data will also make use of
this value when setting default display ranges. It is worth making a reasonable estimate
of this.

Waveform channels also have an (optional) dRate argument. If you omit it, it is set to
the sampling rate of the channel. It can happen that you would have preferred to set a
different sample rate, but the available time base forced a slightly different rate. You
can use this to record the ideal rate you would have preferred.

\subsection PhyChan Physical channel
The channels all have an (optional) iPhyChan parameter that is set to -1 if you do not supply
it. This is not used by the library internally. You are expected to use it to identify the
physical channel used when collecting the data. In Spike2, we use it to decide if a channel
was sampled (set >=0) or created by software (-1) when attempting to reconstruct a sampling
configuration from a data file.

\subsection CreateEventChan Create an Event channel
\ref EventFallRise channels are the simple to create, requiring only the channel number,
the type of event (EventRise or EventFall) and an expected rate.

The following code creates an EventFall channel at 100 Hz on physical event channel 0
\code
#include "s64priv.h"
using namespace ceds64;
TSon64File file;                // A file object
file.Create(fName, 100);        // Create a file, 100 channels, assume no error

int iErr = file.SetEventChan(0, 100.0, EventFall, 0); // chan 0, 100 Hz, phy chan 0
if (iErr == S64_OK)             // You should always check for errors
{
    file.SetChanTitle(0, "Event-");
    file.SetChanComment(0, "Channel 0 comment goes here");
}
\endcode

\subsection CreateLevel Level event channel
\ref LevelEvent channels in the \ref Son64 system are stored as Marker data, using the first
marker code to set the level (0 for low, not 0 for high). You can write data to the channel
as either a stream of times using CSon64File::WriteLevels() that are assumed to alternate low
and high with the channel initial level set by CSon64File::SetInitLevel(), or you can write a
stream of Markers using CSon64File::WriteMarkers().

There are three equivalent ways to create an EventBoth channel:
- CSon64File::SetEventChan() with a channel type of EventBoth. This is provided to be as similar
  as possible to the original \ref Son32 code.
- CSon64File::SetLevelChan()
- CSon64File::SetMarkerChan() with a channel type of EventBoth. EventBoth channels are the same
  as a Marker channel, but with extra features to treat the data as levels.

The following creates channel 1 as a level channel with a mean reate of 5 Hz on physical event
channel 1.
\code
#include "s64priv.h"
using namespace ceds64;
TSon64File file;                // Create a file object
file.Create(fName, 32);         // Create a file, assume no error

int iErr = file.SetLevelChan(1, 5.0, 1);
file.SetChanTitle(1, "Level");
file.SetChanComment(1, "Channel 1 comment goes here");

file.SetInitLevel(1, boolInitLevel); // Set before first WriteLevels() call
\endcode

\subsection CreateMarker Marker channel
Marker channels have all the properties of event channels, plus the ability to store extra marker
codes with each data item. You can then filter the data based on the codes. The
CSon64File::SetMarkerChan() call can also creat EventBoth channels, but we suggest that you use
CSon64File::SetLevelChan() for that as it makes your intent clearer.

Marker channels are often used to save individual keypresses. Spike2 has traditionally used channel
30 (user channel 31) as a keyboard marker channel with marker code 0 holding the (assumed ASCII)
key codes. It also uses channel 31 (user channel 32) as the digital marker channel. Note that
Spike2 user channel numbers start at 1, but the library channel numbers start at 0.

The following creates channel 30 as a keyboard marker with a 1 Hz expected rate. We default both the
channel type and physical channel parameters:
\code
#include "s64priv.h"
using namespace ceds64;
TSon64File file;                // Create a file object
file.Create(fName, 32);         // Create a file, assume no error

int iErr = file.SetMarkerChan(30, 1.0);
file.SetChanTitle(30, "Key");
file.SetChanComment(30, "Keyboard marker channel");
\endcode

\subsection CreateTextMark TextMark channel
A \ref TextMarkData channel holds markers that also have a text string of fixed maximum length
attached to each data item. You can create a TextMark channel using CSon64File::SetExtMarkChan()
but it is usually clearer to create it with CSon64File::SetTextMarkChan(). The text stored is
assumed to be UTF-8 encoded (which is, of course, compatible with ASCII characters 0-127). The
test is terminated by a 0 byte.

In Spike2, user channel 30 has been set as a the TextMark channel, but you can have any number of
them on any channel numbers. The space allocated for each item is rounded up to a multiple of 8
bytes, so you may as well set the size as a multiple of 8 bytes.
\code
#include "s64priv.h"
using namespace ceds64;
TSon64File file;                // Create a file object
file.Create(fName, 32);         // Create a file, assume no error

int iErr = file.SetTextMarkChan(29, 1.0, 80);   // Rate 1 Hz, 80 chars per item
file.SetChanTitle(29, "Text");
file.SetChanComment(29, "Text marker channel");
\endcode

\subsection CreateExtMark Extended marker channels
The general concept of an extended marker type is that each data item is a marker followed
by repeated data items organised in rows and columns. If there is more than 1 column, the
column data is interleaved. Currently only the \ref AdcMarkData channel type uses multiple
columns in Spike2. However, it is perfectly legal to set multiple columns for RealMark
data in your own code; Spike2 will not handle them correctly if you do.

There are currently three types of extended marker channels:
- \ref TextMarkData A marker plus a text string (see \ref CreateTextMark)
- \ref RealMarkData A marker plus a list of IEEE 32-bit floating point values
- \ref AdcMarkData A marker plus 1 or more interleaved 16-bit waveforms

CSon64File::SetExtMarkChan() can create all three types. In theory, we can attach any type of
fixed size data object to a Marker, so we may add more types in the future, or add a more
generalised type for user-defined data.

Note that the AdcMark setup is more complex as we must set the number of traces and the sample
interval for each trace (they are all the same). The interval is set in terms of the file time
base.
\code
#include "s64priv.h"
using namespace ceds64;
TSon64File file;                // Create a file object
file.Create(fName, 32);         // Create a file, assume no error
file.SetTimeBase(0.000001);     // Explicitly set 1 us time base

int iErr = file.SetExtMarkChan(2, 1.0, TextMark, 80);   // Rate 1 Hz, 80 chars per item
file.SetChanTitle(2, "Text");
file.SetChanComment(2, "Text marker channel");

file.SetExtMarkChan(3, 2.0, RealMark, 4);    // Rate 2 Hz, 4 attached items
file.SetChanTitle(3, "RealMk");
file.SetChanComment(3, "RealMark channel with 4 attached values");

// Create an AdcMark channel with 4 interleaved traces of 32 points each on physical
// channel 5 (there is no way to flag more that one channel, so the convention is to
// set the first one). The waveform divisor is 40, which means that the data is sampled
// at 25 kHz. We also set the number of pre-trigger points to 10 (see Spike2 for an
// explanation of this). The timing of each item is the time of the first saved waveform
// point, NOT the time of the trigger (which could be anywhere in the waveform).
iErr = file.SetExtMarkChan(4, 100.0, AdcMark, 32, 4, 5, 40, 10);
file.SetChanTitle(4, "AdcMk");
file.SetChanComment(4, "AdcMark channel with 4 traces of 32 points");
file.SetChanScale(4, 2.0);  // scale from -10 to +9.9997 user units
file.SetChanUnits(4, "mV"); // Assume system has a gain of 1000
file.SetChanOffset(4, 0.0); // This is the default, you could omit this line
\endcode

\subsection CreateWaveForm Waveform (Adc and RealWave) channels
You can create \ref Waveform data of two types:
- Adc (16-bit integer): Integral values in the range -32768 to 32767. These offer efficient
  data storage at the cost of a limited range and resolution.
- RealWave (32-bit float): These offer a much wider dynamic range, but at the cost of twice
  the space on disk.

If the data originates from a 16-bit or less Analogue to Digital Converter (ADC), the use of
16-bit integer data seems quite natural and efficient. You can read Waveform data of either
sort as itself, or as the other type; the library handles the conversion for you using the
channel scale and offset values you set.

The following example creates two channels, one of each data type. The first is an Adc
channel assumed read from a 16-bit ADC sampled at 10 kHz from physical port 19. The second
is a RealWave channel sampled at 1 kHz with no physical port. Waveform channels have an
lDvd argument that sets the extact sample interval as a multiple of the file time base.
They also have a dRate parameter that you can set to 0.0 (meaning work it out from lDvd)
or you can set it to the rate that you actually wanted. This argument is not used internally
by the library.
\code
#include "s64priv.h"
using namespace ceds64;
TSon64File file;                // Create a file object
file.Create(fName, 32);         // Create a file, assume no error
file.SetTimeBase(0.000001);     // Explicitly set 1 us time base

// Create Adc channel. Set the channel divide to 100 (being 100 us, which is 10 kHz).
// As this is exactly the desired rate, we set dRate to 0.0 so the system will calculate
// the actual rate for us.
int iErr = file.SetWaveChan(5, 100, Adc, 0.0, 19);
file.SetChanTitle(5, "ADC16");
file.SetChanComment(5, "Waveform channel as 16-bit ADC data");
file.SetChanScale(5, 1);        // Range: -5 to 4.9998 units (offset defaults to 0)
file.SetChanUnits(5, "mV");     // set the units

// Create a RealWave channel. We want 1000 Hz, so set the divisor to 1000
// The scale and offset default to 1 and 0. By leaving them as this, we are saying
// that we expect the used range to be -5 to 4.9998 Volts. This does not limit the range
// in any way unless you read the data as 16-bit integers.
file.SetWaveChan(6, 1000, RealWave);    // Rate 1000 Hz
file.SetChanTitle(6, "Wave");
file.SetChanComment(6, "Waveform channel as 32-bit floating point data");
\endcode

\section SetBuffer Set the data buffering
When you open an old file for read, each channel has a read buffer that is used to hold the
last block of data read and each channel also keeps an index tree to this last read block.
If you should write to such a file, we also generate a write buffer and write tree index 
that points to the end of the channel.

When you open a new file for writing, we also make provision for each channel to have a
'circular' write buffer to hold the most recent written data. This buffering is designed
to make it easy for a user of the library to set a channel as 'not writing to disk' and
yet still be able to read data from it for display or analysis purposes. It also allows
you to mark sections of data retrospectively for saving.

The \ref Son32 system _requires_ CSon64File::SetBuffering() to be called before you write
data to a channel (things will go badly wrong if you do not call it). It is optional in
the \ref Son64 system. The simplest course is to always call it. Note that the \ref Son32
system implements a buffering system based on 32 kB data chunks that are all written or
not written. Setting a Son32 channel to no buffering actually allocates a single buffer.

The \ref Son64 system does not require SetBuffering() to be called. If you do set a buffer
you can set arbitrary time ranges for saving or not saving and the library will save
exactly the data you specify.

You should set the buffering after creating the channels and before writing any data to
them. You can set buffering for each channel individually, or for all the channels at one
time. You can set buffering based on either the number of bytes of memory to allocate to
buffering, or on the length of time you would like to buffer for, or for a combination of
the two. To work out how much space to allocate for event-based channels (which do not have
a fixed data rate) we use the dRate parameters that you supplied when creating the channel.

The following code gives an example of how to set the buffering. It shows 3 ways you could
set the buffering for all channels. In your code you would only use one of them. The
example sets all the channel, then sets some channels individually. If you wanted, you
could omit the all channels code and then set all channels individually. The only danger
with this is that you might forget a channel, which is a problem for TSon32File files.

\code
#include "s64priv.h"
using namespace ceds64;
TSon64File file;                // Create a file object
file.Create(fName, 32);         // Create a file, assume no error

...                             // Setup the file, create the channels

// Set all channels based on total buffer size or buffer time (3 ways shown)
double dSeconds;
dSeconds = file.SetBuffering(-1, 8000000);      //1. total buffer size of 8 MB
file.SetBuffering(-1, 0, 3.0);                  //2. set buffering for 3 seconds
dSeconds = file.SetBuffering(-1, 8000000, 3.0); //3. lesser of 3 secomds or 8 MB

// Having set all the channels, you can customise individual ones. The system
// saves the buffering time when you set all the channels. Set the time to -1
// to use the saved buffering time. To disable circular buffering for a
// channel, set to both the time and the buffer size to 0.
file.SetBuffering(27, 0, 0);    // cancel buffering for channel 27
file.SetBuffering(6, 0, 10);    // set 10 seconds buffering for channel 6

// Now you can write data
\endcode

\section WrData Writing the data
Once the channels have been created and the buffering set you can write data to the file.
You can write channels of data in any order and in any quantity of data at a time, but the
data must be written in ascending time order (exceptions: waveform data can overwrite old
waveform data - e.g. for filtering, CSon64File::EditMarker() can be used to modify Marker
data).

If you have continuous streaming input data, but you only want to save certain sections of
it, you have two choices:
1.	Write only the sections of data that you want to appear in the file.
2.	Write all available to the file then use the \ref GpBuffering functions to tell the 
    library which data should and should not be saved and included in the final disk file.

Using the second option allows the library optimise internal operations and
to ensure that disk space is used efficiently. It also increases application flexibility by
allowing the save/discard decision to be made retroactively and by making data that is
eventually discarded temporarily appear to be part of the file for display purposes.

When data is written to a channel and needs to be written to disk, space is allocated at the
current end of the file. This means that when streaming data in real time, data for around
the same time in very large files will be at a similar position in the file. If you write
code to copy a very large file, although it is easiest to copy all one channel, then the
next, and so on, this leads to a target file where all the data for any particular time is
spread over the entire file. It is usually better to write data time range by time range
over all channels so that all the data at a particuular time will tend to be at a similar
position in the file.

\subsection WrWave Writing waveform data
Waveform channels hold repeated data of the same time that is sampled at a fixed time interval.
This interval is defined by the channel divide, tDivide, which you can read using
CSon64File::ChanDivide(). The sample interval in seconds is tDivide * the base time unit for
the file. A waveform channel can have gaps, which are places where the time between
one data item and the next is not tDivide.

To write waveform data you need an array of short for Adc channels and an array of float for
RealWave channels. To write data, you provide an array of data of the correct type for the
channel and the time of the first array element. Once you have written a data block, this
sets the data aligment for the channel. All future writes are expected to start at a time that
is offset from the start time by an integral number of tDivide.

When the last data item in a waveform channel is at time t, if the next write past time t starts
at time t + tDivide the data is contiguous. However, if the data is not at this time, we add a
gap into the data stream and start a new data block.

If you write waveform data that overlaps previously written data, the previous data is overwritten.
If the times do not match exactly, the nearest data point is overwritten. If you write data that
would fall into a gap, the data for the gap is ignored. The only way to repair gaps is to copy the
data with gaps to a new channel.

The following code example creates and writes blocks of waveform data. We create an Adc channel
sampled at 10 kHz on channel 0 and a RealWave channel at 1 kHz on channel 1. We write 10 data
blocks of 1 second each with no gaps. It accepts the default values for comments, units, scales
and offsets.
\code
#include "s64priv.h"
using namespace ceds64;
TSon64File file;                // Create a file object
file.Create(fName, 32);         // Create a file, assume no error
file.SetTimeBase(0.000001);     // Explicitly set 1 us time base
int iErr = file.SetWaveChan(0, 100, Adc, 0.0, 19);  // Rate 10 kHz
iErr = file.SetWaveChan(1, 1000, RealWave);         // Rate 1000 Hz
file.SetBuffering(-1, 0, 10.0); // Set 10 seconds of buffering

std::vector<short> vAdc(10000); // 10000 adc points - 1 second
std::vector<float> vReal(1000); // 1000 RealWave points - 1 second

TSTime64 t0 = 0;                // start time of channel 0
TSTime64 t1 = 0;                // start time of channel 1
for (int i=0; i<10; ++i)        // write 10 blocks of each channel
{
    ... fill buffers with data
    t0 = file.WriteWave(0, vAdc.data(), vAdc.size(), t0);   // Write channel 0 data
    if (t0 < 0)                 // check for error
        ...handle error...
    t1 = file.WriteWave(1, vReal.data(), vReal.size(), t1); // Write channel 1 data
    if (t1 < 0)
        ...handle error...
}
file.Close();                   // explicitly close the file
\endcode

\subsection WrEvent Writing event data
Event data is passed around as arrays of TSTime64 values (64-bit signed integers). Data
must always be written in ascending order, and it is assumed that all arrays passed to the
writing routines are in ascending time order. Further, you are only allowed to write event
data to the end of the channel, so the first time you write must also be at a greater time
that the last time written or you will get an OVER_WRITE error.

If you want to modify event data, the only way is to read the channel and write a new channel
holding modified data.
\code
#include "s64priv.h"
using namespace ceds64;
TSon64File file;                // Create a file object
file.Create(fName, 32);         // Create a file, assume no error
file.SetTimeBase(0.000001);     // Explicitly set 1 us time base
int iErr = file.SetEventChan(2, 100.0);  // Create EventFall channel 2
file.SetBuffering(-1, 0, 0);    // Set no buffering

std::vector<TSTime64> vEvt(1000); // Space for 1000 event times

TSTime64 t = 0;                 // start time of channel
for (int i=0; i<10; ++i)        // write 10 blocks 
{
    // Generate some data (events at 100 us intervals)
    for (size_t n = 0; n < vEvt.size(); ++n)
        vEvt[n] = t, t += 100;  // Fill with equally spaced events

    // Write the events to the data file
    iErr = file.WriteEvents(2, vEvt.data(), vEvt.size());
    if (iErr < 0)               // check for error
        ...handle error...
}
file.Close();                   // explicitly close the file
\endcode

\subsection WrMarker Writing Marker data
A TMarker is composed of a time (TMarker::m_time, 8 bytes long) followed by 8 bytes that
can be referred to aseither an array of 8 unsigned 8-bit integers, two 32-bit signed
integers or 1 64-bit signed integer. The \ref Son32 library uses the ced32::TMarker type
that is a 32-bit time, followed by 4 8-bit marker codes. Spike2 currently uses TMarker as
if it were a time plus 4 marker codes and sets the second 4 codes to 0.

Markers are written in exactly the same way as event data is written. The only difference
being that the data type is TMarker not TSTime64 as used for events.
\code
#include "s64priv.h"
using namespace ceds64;
TSon64File file;                // Create a file object
file.Create(fName, 32);         // Create a file, assume no error
file.SetTimeBase(0.000001);     // Explicitly set 1 us time base
int iErr = file.SetEventChan(2, 100.0);  // Create EventFall channel 2
file.SetBuffering(-1, 0, 0);    // Set no buffering

std::vector<TMarker> vMark(1000); // Space for 1000 markers

TSTime64 t = 0;                 // start time of channel
uint8_t uCode = 0;              // a code value to write
for (int i=0; i<10; ++i)        // write 10 blocks
{
    // Generate some Marker data (at 100 us intervals)
    for (auto& m : vMark)       // for each TMarker in the vector
    {
        m.m_time = t;           // set the time
        m.m_int64 = 0;          // zero all marker code space
        m.m_code[0] = uCode;    // set first marker code
        t += 100;               // Fill with equally spaced events
        uCode += 1;             // will wrap around from 255 to 0
    }

    // Write the events to the data file
    iErr = file.WriteMarkers(2, vMark.data(), vMark.size());
    if (iErr < 0)               // check for error
        ...handle error...
}
file.Close();                   // explicitly close the file
\endcode

\subsection WrExtMark Writing extended marker data
Extended marker types are a TMarker followed by a block of data that is a multiple of 8 bytes
in size. The data block following the marker holds a text string for a TTextMark, an array of
floats for a TRealMark and an array of short (or interleaved arrays of short) data for TAdcMark
data.

In principle, writing extended markers is identical to \ref WrMarker and \ref WrEvent, however
there is a significant difference: you cannot create an array or vector of extended markers
because they do not have a fixed size. The size is fixed for each channel, of course, and
CSon64File::ItemSize() returns this size in bytes.

The types, TTextMark, TRealMark and TAdcMark are of fixed size, but this fixed size only
covers one appended item and these types are really just placeholders. Each of these types
can return a pointer to the start of the added data (which will be the address of the object
+ sizeof(TMarker), which is 16 bytes).

This means that to work with arrays of these objects, you must allocate memory and do your
own pointer arithmetic. We do provide the db_iterator<T, itemSz> type to make this easier.
The first example writes TTextMark data doing pointer manipulation by hand:
\code
#include "s64priv.h"
using namespace ceds64;

// Utility routine to fill in a text mark item
void SetTextMark(TTextMark& tm, TSTime64 t, uint8_t code, const char* szText, int nMax)
{
    tm.m_time = t;              // set the time
    tm.m_int64 = 0;             // zero the attached data
    tm.m_code[0] = code;        // set the first code
    char* pDest = tm.Chars();   // get pointer to text buffer
    strncpy(pDest, szText, nMax);   // copy up to nMax chars
    pDest[nMax-1] = 0;          // ensure terminated
}

TSon64File file;                // Create a file object
file.Create(fName, 32);         // Create a file, assume no error
file.SetTimeBase(0.000001);     // Explicitly set 1 us time base
int iErr = file.SetTextMarkChan(30, 1.0, 80);  // Create TextMark with 80 character buffer
file.SetBuffering(-1, 0, 0);    // Set no buffering

int nBytesPerItem = file.ItemSize(30);  // get bytes - will be 96
const int nInBuff = 100;        // Space for 100 items
void* pVoid = malloc(nInBuff * nBytesPerItem);  // space needed
if (!pVoid)
    ...handle out of memory...

TTextMark* pTM = static_cast<TTextMark*>(pVoid);    // point at first item
char szWork[80];                // space to build strings
for (int i=0; i<nInBuff; ++i)   // set all the items
{
    sprintf(szWork, "This is text marker %d", i);   // make a string
    SetTextMark(*pTM, i*1000, (uint8_t)i, szWork, nBytesPerItem);
    pTM = reinterpret_cast<TTextMark*>(static_cast<char*>(pTM) + nBytesPerItem);
}

// Write the TTextMark data to the file
iErr = file.WriteExtMarks(30, static_cast<TExtMark*>(pVoid), nInBuff);
if (iErr < 0)                   // check for error
    ...handle error...

free(pVoid);                    // Don't forget to release the malloc memory
file.Close();                   // explicitly close the file
\endcode

If we wanted to use db_iterator we could write the memory manipulation as:
\code
#include "s64iter.h"
using namespace ceds64;
...
std::vector<char> vWork(nInBuff * nBytesPerItem);   // allocate work space
db_iterator<TTextMark> it(reinterpret_cast<TTextMark*>(vWork.data()), nBytesPerItem);
for (int i=0; i<nInBuff; ++i, ++it)                 // set all the items
{
    sprintf(szWork, "This is text marker %d", i);   // make a string
    SetTextMark(*it, i*1000, (uint8_t)i, szWork, nBytesPerItem);
}
\endcode

Note that we need not provide the second template argument to db_iterator because the
TTextMark type is known to the db_iterator template as one with variable size (see the
s64iter.h file for details of known classes).

Working with TAdcMark and TRealMark channels follows the same pattern. For TAdcMark
items, the Shorts() member function returns a pointer to the data. For TRealMark the
Floats() member identifies the data.

\section WrThread Threading when writing data
If you will only be using this library from a single thread of execution (that is, you
do not deliberately create threads to handle tasks) you can ignore this section.

The library is designed to be thread safe in the sense that once you have created a data
file for writing, you can have multiple threads of execution writing and reading data.
There are various levels of mutex used to protect the file. The most fundamental one
ensures that only one thread can read or write to the underlying operating system file
at any one time.

As you will most likley be passing pointers to the file to each thread, it is your
responsibility to make sure that the pointers remain valid until all threads have finished
with the file.

The channels in a file are independent of each other, so you can have a thread writing to
one channel while another thread reads from a different channel. The system is arranged
so that you will only need to wait for another thread when the activities of the threads
are incompatible. For example, two separate threads that both read separate channels will
proceed independently until the both want to read from the disk at the same time. At this
point, one will get the disk access mutex and the other will have to wait. We are not
aware of and deadlock situations in the library.

You can read from a channel that is being written to by another thread. In this case the
write thread has priority. However, we maintain separate structures for read and write
which can greatly minimise contention (so that disk indices are not constantly being
updated between reads in one place and writes at the end of the channel).
*/

//-----------------------------------------------------------------------
/*!
\page FileReadOnly Read only files
Read only files can be used as data sources and behave identically to read/write
files as long as you do not perform any operation that changes the file. However,
many operations only change the file and channel headers in memory and rather than
give an immediate error, we mark the appropriate memory structure as modified and
defer writing (and any READ_ONLY error) until we need to use CSon64File::Commit()
to make the changes permanent.

It is sometimes useful to be able to make 'local' modifications to a read only file,
usually when you want to copy it to a new file.
The most likely things that you would change in a read-only file are the file time
base with CSon64File::SetTimeBase() and channel scales and offsets with
CSon64File::SetChanScale() and CSon64File::SetChanOffset().

The CSon64File::Close() routine (which would usually try to update modified structures)
detects that the file is read only and skips the writing phase, so does not return
a READ_ONLY error after such changes.

Of course, operations that trigger disk writes (for example deleting or creating
channels) will return an immediate error and do nothing.
*/

//-----------------------------------------------------------------------
/*!
\page PgWrBuf The circular write buffer and committing data
\tableofcontents
When you create a new data file, extra code is enabled for each data channel to allow the
use of circular buffering of the data before it is added to the disk file. You can control
the size of the channel buffers by time or memory use with CSon64File::SetBuffering() and
even disable it. Having circular buffers has benefits:
- You can retrospectively mark data for saving based on the data content
- You can read back data from the circular buffer just like any other data
- You can get the lists of saving/not saving times for display purposes

However, it also has a downside. The larger your circular buffering, the more data you
will lose in the case of a disaster (loss of power, system crash). You do not need to
use the circular buffering. If you do not want it, call CSon64File::SetBuffering(-1, 0)
after creating the channels and before writing data.

A related library feature is the ability to make sure that data that we know we want to
save has been committed. This is usually done with the CSon64File::Commit() command.

\section WrBuffer Using the circular buffer
In Spike2, we use the circular buffering system to store recently sampled data in memory.
If the user turns off writing to disk, they can still view the data until if falls out of
the circular buffer. It also means that they can do some on-line data analysis in the
script language to decide if the data is interesting or not and mark it for saving if it
is.

For example, suppose we have several waveform channels that are sampling at a high rate,
monitoring a specimen in a long term test. Most of the time, nothing happens, but every
now and again there is activity and we have a way to detect activity. We only want to
save data when something happens. To do this:

1. Set up the file and the channels ready to write.
2. Set up the channel buffering on the high-speed channels to last long enough into the
   past so we can detect the activity reliably.
3. Use CSon64File::Save() to set these channels to not saving.
4. Stream all the data to the file. Ideally this is done using a thread of execution that
   is devoted to this activity, but it could also be done in a system that polls a series
   of tasks and runs whatever task (write or detect activity) is ready.
5. Have a background activity monitor (this could be a separate thread) that detects the
   desired activity. If the activity is detected, use CSon64File::SaveRange() to mark the
   time range of data to be saved.

When you create a new data file with CSon64File::Create() you can choose to add a 
circular buffer to each or selected channels with the CSon64File::SetBuffering() command.
Once a channel has a circular buffer, you can use CSon64File::Save() and
CSon64File::SaveRange() to mark data in the circular buffer for saving or not saving.
Data in the circular buffers is always available for reading, regardless of the save/no
save state of individual data values. You can retrieve the list of times where the save
state changes in the circular buffer with the CSon64File::NoSaveList() command.

If you have a circular buffer set for a channel, the CSon64File::Commit() command is
modified. Only data that is marked for saving is committed. Further, once data is
committed, you cannot change the save/no save state of data that is already committed.

To make effective use of the circular buffer you will normally set a channel to the no
save state with the CSon64File::Save() command, then use the CSon64File::SaveRange()
command to mark regions for saving. Notice that the circular buffer allows you too make
decisions about saving or not saving for as long as the data remains in the circular
buffer and you have not committed any later data.

If you use a circular buffer, you should also consider calling CSon64File::LatestTime()
to give the library the opportunity to clean up the save/no save list.

\subsection WrBufferSon32 Circular buffering in Son32 files
If you are using a \ref Son32 file (through the TSon32File class), the circular buffer
is less potent.

A \ref Son64 channel saves exactly the time ranges of data that are specified.
A \ref Son32 file has a set of buffers that each match the size of the channel
disk buffer (usually 32 kB). It also has a list of up to 8 times at which the save
to disk state changes. Each of the set of buffers can be saved or not saved -
either the entire buffer is saved, or none of it is saved.

All the commands exist and work as well as they can withing the limits that only full
buffers of data can be marked for save or no save and the maximum number of changes of
save state per channel is set by the Son library to (currently) 8.

You _must_ call TSon32File::SetBuffering() for all channels in a \ref Son32 file, even
if you do not want to use circular buffering.
*/

}; // End of namespace hack for DOxygen
