// s64st.cpp
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

//! \file s64st.cpp
//! \brief Implementation of the CSaveTimes class used in the circular buffers
//! \internal

#include <assert.h>
#include "s64st.h"

using namespace ceds64;
using namespace std;

/*!
\class CSaveTimes
\brief Class to manage the list of times that data should be saved for
\internal
This is a class that handles a list of times and whether we are saving or not saving
data for a particular time. The class has the first time we care about and the saving
state at that time, plus a vector of times (in order) at which the state changes.
The final save state is assumed to continue to infinite time (TSTIME64_MAX at any rate).
    
If m_bFirstSaved is true, start time is 4 and the list held:<br>
    8   23<br>
We would be saving 4-7, and 23 onwards.

We store the list of times in a vector as we suspect that there will never be many entries
and a vector is generally more efficient that a list. We also suspect that we will tend to
add ranges at the end, so a vector will not be too bad in use.

Calls to this class must be serialised; it is not re-entrant.
*/

//! Reset back to the constructed state
void CSaveTimes::Reset()
{
    m_vTimes.clear();           // No saved times
    m_tStart = 0;
    m_bStartSaved = true;
    m_nFetch = 0;
}

//! Get the overall state of the list of times
/*!
If the list is empty, then we are either saving all or saving none, otherwise we are saving
some items.
\return eST_all, eST_none or eST_some.
*/
CSaveTimes::eST CSaveTimes::State() const
{
    if (m_vTimes.empty())
        return m_bStartSaved ? eST_all : eST_none;
    else
        return eST_some;
}

//! Change the time of the first item in the list.
/*!
This tells the list the time of the oldest thing we are interested in, that is changes
before this time are no longer interesting and should be flushed from the list.

This may cause items to be removed and it may also change the first saved state. If
we remove an odd number of items, the saving state of the first item is inverted.
If m_bFirstSaved is true, start time is 4 and the list holds 8, 23:<br>

   t   | Action
------ | ---------------
 <=4   | Makes no change
 5-7   | Change the start time
8-22   | Change start time, delete first change time, invert save state
23-    | Change start time, clear change times, no state change

\param t    The oldest time we are interested in
*/
void CSaveTimes::SetFirstTime(TSTime64 t)
{
    if (t <= m_tStart)              // if this causes no change...
        return;                     // ...we are done
    m_tStart = t;                   // replace the start time
    if (!m_vTimes.empty())
    {
        auto it = upper_bound(m_vTimes.begin(), m_vTimes.end(), t);  // first un modified time
        auto nRemove = it - m_vTimes.begin();   // numer of items to erase
        m_vTimes.erase(m_vTimes.begin(), it);   // remove items that are no longer in the time range
        m_bStartSaved ^= (nRemove & 1) != 0;
    }
}

//! Tell the list about a time range where not data can be found
/*!
If a channel (usually event-based) collects no data for a long time, the list of save/no-save
can become unreasonably long, to no purpose. If the sampling system can tell us (every now and
again) where we have got to, so we know that no more data is possible, we can amalgamate changes
that will have no effect on the data that is saved. We must preserve the parity of the save/no
save markers, so can only delete all, or an even number.

However, some drawing modes (e.g. events as lines) can show us the save/no-save state between
data points, so some channels will want to preserve a reasonable number of changes.
\param tLastData    The last data time in the buffer (or -1 if no data)
\param tReached     A time for which all data is complete, any new data is after this time
\param nKeep        The number of changes to preserve (0 if omitted).
*/
void CSaveTimes::SetDeadRange(TSTime64 tLastData, TSTime64 tReached, int nKeep)
{
    if ((tReached > tLastData) && (nKeep >= 0))
    {
        if (m_vTimes.empty())           // there is nothing to do
            return;
        auto i1 = upper_bound(m_vTimes.begin(), m_vTimes.end(), tLastData);  // first after
        auto i2 = upper_bound(i1, m_vTimes.end(), tReached);    // next after tReached
        auto nRemove = i2 - i1;         // number to be erased
        if (nRemove > nKeep+1)          // Are we over the limit for dead ranges?
        {
            nRemove -= nKeep;           // We will delete the oldest ones
            if (nRemove & 1)            // if an odd number to remove...
                --nRemove;              // ...make even (nRemove will be at least 3)
             m_vTimes.erase(i1, i1+nRemove); // Reduce to a smaller number of changes
        }
    }
}

//! Set the state from time t onwards to be bSave.
/*!
This sets a continuous state from a given time onwards into the future.
Setting the state overrides any later times already set in the list, so any items at
or after the time are deleted. If the state matches the state at the time t, then
we don't need to add t to the list.

See also: SeaveRange()
\param t The time at which we want to set the change. If this is less than or equal to
         the current start time, the list is cleared and this defines the start state.
\param bSave The new state from time t onwards.         
*/
void CSaveTimes::SetSave(TSTime64 t, bool bSave)
{
    if (t <= m_tStart)                  // we have no interest in times before the...
    {                                   // ...first, so this means keep the start time...
        m_bStartSaved = bSave;          // ...set whatever state is required...
        m_vTimes.clear();               // ...and remove all changes
        return;
    }

    // To get here, t affects items in the list
    auto it = lower_bound(m_vTimes.begin(), m_vTimes.end(), t); // we will erase from it onwards
    auto nLeft = it - m_vTimes.begin(); // see what we would have left
    m_vTimes.erase(it, m_vTimes.end()); // remove the rest

    // Our list now does not contain any time at or after t. If the state at the previous
    // change matches bSave, we are done, else we must add t to the end. That is, if the
    // original state xor (nLeft is odd) == bSave we are done.
    if ((m_bStartSaved != ((nLeft & 1) != 0)) != bSave)
        m_vTimes.push_back(t);          // set a new change time
}

//! Mark a time range for saving
/*!
Say that we want to save data between the times tFrom up to, but not including the time
tUpto. Think of this a an OR of saving, that is if we are already saving, it makes no
difference. If we are not already saving at time tFrom, we start saving.
\param tFrom  The start time of the range to be saved
\param tUpto  The end of the range (not included in the range)
*/
void CSaveTimes::SaveRange(TSTime64 tFrom, TSTime64 tUpto)
{
    if ((tUpto <= m_tStart) ||          // cannot change before fiirst known time
        (tFrom >= tUpto))               // empty time range (can happen if commit wrote
        return;                         // some data in the circular buffer)

    if (tFrom < m_tStart)               // make sure that tFrom is in a sensible range
        tFrom = m_tStart;

    // We will now remove all markers from tFrom up to and including tUpto
    auto i1 = lower_bound(m_vTimes.begin(), m_vTimes.end(), tFrom); // first at or after
    auto i2 = upper_bound(i1, m_vTimes.end(), tUpto);
    auto n1 = i1 - m_vTimes.begin();    // index of first to remove (could be past end)
    auto n2 = i2 - m_vTimes.begin();    // index of last to remove
    bool b1 = m_bStartSaved != ((n1 & 1) != 0); // state at first to remove
    bool b2 = m_bStartSaved != ((n2 & 1) != 0); // state at last preserved
    auto it = m_vTimes.erase(i1, i2);           // remove unwanted items

    // Insert in time order to minimise the movement of items and maximise the chance
    // to be inserting at the end of the vector.
    if (!b1)                            // if initial in wrong state...
    {
        it = m_vTimes.insert(it, tFrom);// ...insert the initial time
        ++it;                           // make the next insertion point
    }
    if (!b2)                            // if final not saving...
        m_vTimes.insert(it, tUpto);     // ...we must add a time
}

//! Get a range of time values to be saved
/*!
This gets the first range of data to be saved with a maximum time limit on what we are
interested in.

\param pFrom    Points at start time for a range of values to be saved.
\param pUpto    Points at the (not included) end time of the range.
\param tUpto    A time limit for the end of the range
\param tFrom    Default is -1. The start time of the search.   
\return         true if there is a range of data to be saved, false if not.
\sa NextSaveRange()
*/
bool CSaveTimes::FirstSaveRange(TSTime64* pFrom, TSTime64* pUpto, TSTime64 tUpto, TSTime64 tFrom) const
{
    if (tUpto <= m_tStart)
        return false;

    bool bGotRange;

    if (m_bStartSaved)
    {
        *pFrom = m_tStart;              // first time
        TSTime64 t = m_vTimes.empty() ? TSTIME64_MAX : m_vTimes[0];
        *pUpto = (t > tUpto) ? tUpto : t;
        m_nFetch = 1;                   // next start index
        bGotRange = true;               // we have a range
    }
    else
    {
        m_nFetch = 0;                   // next start index in vector
        bGotRange = NextSaveRange(pFrom, pUpto, tUpto);
    }

    // See if we need to skip past ranges that are already saved
    while (bGotRange && (*pFrom < tFrom))  // We must limit the time
    {
        if (*pUpto > tFrom)             // if we want this range
            *pFrom = tFrom;             // reset the start and we are done
        else
            bGotRange = NextSaveRange(pFrom, pUpto, tUpto);
    }

    return bGotRange;
}

//! Get Subsequent ranges to save
/*!
Call this after FirstSaveRange to collect subsequent ranges. Where we have
reached is saved internally and to work you must not change the underlying
object, or you must call FirstSaveRange again.

See also: FirstSaveRange()
\param pFrom    Points at start time for a range of values to be saved.
\param pUpto    Points at the (not included) end time of the range.
\param tUpto    A time limit for the end of the range
\return         true if there is a range of data to be saved, false if not.
*/
bool CSaveTimes::NextSaveRange(TSTime64* pFrom, TSTime64* pUpto, TSTime64 tUpto) const
{
    if (m_nFetch >= m_vTimes.size())    // if we are off the end...
        return false;                   // ...no data to save, so done
    TSTime64 t = m_vTimes[m_nFetch];    // Collect the start time
    if (t >= tUpto)
        return false;
    *pFrom = t;
    t = m_nFetch+1 >= m_vTimes.size() ? TSTIME64_MAX : m_vTimes[m_nFetch+1];
    *pUpto = (t > tUpto) ? tUpto : t;
    m_nFetch += 2;                      // next range starts here, if it exists
    return true;
}

//! Report if the state at the end of the list is saving or not
/*!
\param tAt   The time at which we want to know if we are saving or not. If this is before
            the oldest time in the buffer we assume that the result is the same as the initial
            state, else it alternates...
\return true if data added at times at or beyond tAt would be saved.
*/
bool CSaveTimes::IsSaving(TSTime64 tAt) const
{
    auto it = std::upper_bound(m_vTimes.begin(), m_vTimes.end(), tAt);
    auto i = it - m_vTimes.begin();
    return m_bStartSaved != ((i & 1) != 0);
}

//! Get a list starting with the first off time
/*!
The list of times is a dynamic thing, but we may want to indicate when drawing that data is
to be saved or not saved. We return a list of times, starting with an off time, of the
changes in saving state that correspond with data in the circular buffer. If saving is off
at tFrom, we return tFrom as the first time.
\param  pTimes  Points at space to hold the returned values
\param  nMax    The maximum number of items to return. If this is 0, return # changes
\param  tFrom   The first time to consider
\param  tUpto   We are not interested in times at or past this.
\return The number of items that we would like to put in the list. 0 means always saving. If
        this is greater than nMax then we have returned nMax. Beware async additions.
*/
int CSaveTimes::NoSaveList(TSTime64* pTimes, int nMax, TSTime64 tFrom, TSTime64 tUpto) const
{
    if ((tUpto <= m_tStart) ||      // if this is before the first time we know of...
        (tUpto <= tFrom))           // ...or no time range
        return 0;                   // ...no changes that we know about, assume writing

    if (m_vTimes.empty())           // if the list of times is empty
    {
        if (m_bStartSaved)          // if we are saving, then there is...
            return 0;               // ...no information to return
        if (--nMax >= 0)
            *pTimes++ = (tFrom < m_tStart) ? m_tStart : tFrom;
        return 1;
    }

    // To get here, there are times in the list and tUpto > m_tStart
    TSTime64 tLastChange = m_vTimes.back();
    if (tLastChange<= tFrom)        // we are past the final change
    {
        bool bSaving = m_bStartSaved != ((m_vTimes.size() & 1) != 0);
        if (bSaving)                // if all saving, then...
            return 0;               // ...nothing to be said
        if (--nMax >= 0)
            *pTimes++ = tFrom;
        return 1;
    }

    // To get here, there is a list of times and we overlap with it. nMax and
    // pTimes are unchanged, we have put nothing into the list. We find the first
    // change past the start time. If this is turning on we have an off to deal with
    // at the start. Further, the first interval is special.
    int nRet = 0;
    auto it = std::upper_bound(m_vTimes.begin(), m_vTimes.end(), tFrom);
    auto index = it - m_vTimes.begin();  // index of first change after tFrom

    // if (starting saved and index is odd), previous is no save, add start time
    // if (staring unsaved and index is even), previous is no save, add start time
    if (m_bStartSaved != ((index & 1) == 0)) // If we need to put in an off time at start
    {
        if (index == 0)             // if this is the first, then we may need m_tStart
        {
            if (m_tStart > tFrom)   // If turn off after tFrom, we must...
                tFrom = m_tStart;   // ...record the off time as first time
        }

        if (--nMax >= 0)
            *pTimes++ = tFrom;
        nRet = 1;                   // we have 1 value to return
    }

    // We may or may not have included an initial off time, but we want the rest of the
    // list times that are less than tUpto.
    while ((it != m_vTimes.end()) && (*it < tUpto))
    {
        if (--nMax >= 0)
            *pTimes++ = *it;
        ++nRet;
        ++it;
    }

    return nRet;                    // return the number copied
}

#ifdef TESTME
// Currently just so we can step through and check the result
int testSaveTimes()
{
    CSaveTimes st;
    st.SetSave(0, true);           // set all saving
    st.SetSave(100, false);
    st.SaveRange(200, 300);
    st.SaveRange(250, 270);         // no change
    st.SaveRange(400, 500);         // new range
    st.SaveRange(300, 400);         // heal up the gap
    st.SetFirstTime(250);
    return 0;
}
#endif
