/*****************************************************************************
**
** sonex.h
**
******************************************************************************
**
**  Definitions of extra functions for the SON filing system, not yet added
**  to the library proper.
**
*/

#ifndef __SON__
#include "son.h"
#endif

#ifndef __SONEX__
#define __SONEX__


#ifdef __cplusplus
extern "C" {
#endif

/*
** These two functions are intended to be used to find out about channel data
**  without reading it all. SonNextSection is used to find contiguous sections
**  of waveform data, SonItemCount returns the count of data items, primarily
**  for event type channels.
*/
short SonNextSection(short fh, WORD chan, TSTime* psTime, TSTime* peTime);
long  SonItemCount(short fh, WORD chan);

#ifdef __cplusplus
}
#endif

#endif
