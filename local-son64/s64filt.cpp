// s64filt.cpp
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

//! \file s64filt.cpp
//! \brief Implementation of the marker filter system

#include <assert.h>
#include "s64priv.h"
#include "s64chan.h"
#include "s64filt.h"

using namespace ceds64;
using namespace std;

void TMask::set()
{
    fill_n(m_mask.begin(), static_cast<int>(NItem), 0xffffffff);
}

void TMask::set(size_t n)
{
    const size_t i = n >> IndexShift;
    assert(i < NItem);
    m_mask[i] |= (1 << (n & IndexMask));
}

void TMask::reset()
{
    fill_n(m_mask.begin(), static_cast<int>(NItem), 0);
}

void TMask::reset(size_t n)
{
    const size_t i = n >> IndexShift;
    assert(i < NItem);
    m_mask[i] &= ~(1 << (n & IndexMask));
}

void TMask::flip()
{
    for_each(m_mask.begin(), m_mask.end(), [](uint32_t& v){v = ~v;});
}

void TMask::flip(size_t n)
{
    const size_t i = n >> IndexShift;
    assert(i < NItem);
    m_mask[i] ^= (1 << (n & IndexMask));
}

bool TMask::test(size_t n) const
{
    const size_t i = n >> IndexShift;
    assert(i < NItem);
    return (m_mask[i] & (1 << (n & IndexMask))) != 0;
}

bool TMask::operator[](size_t n) const
{
    const size_t i = n >> IndexShift;
    assert(i < NItem);
    return (m_mask[i] & (1 << (n & IndexMask))) != 0;
}

bool TMask::none() const
{
    for (size_t i = 0; i<NItem; ++i)
        if (m_mask[i])
            return false;
    return true;
}

bool TMask::all() const
{
    for (size_t i = 0; i<NItem; ++i)
        if (m_mask[i] != 0xffffffff)
            return false;
    return true;
}

//! Default constructor
/*!
\ingroup GpFilter
This sets all layers as fully set, AND mode and 4 layers, all traces.
*/
CSFilter::CSFilter()
    : m_mode( eM_and )
    , m_nLayers( 4 )
    , m_nColumn( -1 )
    , m_active( eA_all )
{
    std::for_each(m_mask.begin(), m_mask.end(), [](TMask& bs){bs.set();});
}

//! Return the current filter mode 
/*!
\ingroup GpFilter
\return The current filter mode as one of: eM_and, eM_or
*/
CSFilter::eMode CSFilter::GetMode() const
{
    return m_mode;
}

//! Set the current filter mode
/*!
\ingroup GpFilter
\param mode The new filter mode, one of: eM_and, eM_or
*/
void CSFilter::SetMode(eMode mode)
{
    m_mode = mode; m_active = eA_unset;
}

//! Test if two filter masks are identical
/*!
\ingroup GpFilter
Filters are considered identical if they have the same number of layers,
are in the same mode and the masks are the same. This does not check if they
have the same effect. We should consider if we should add this as a check if
the active state is eA_all or eA_none.
\param rhs The filter to compare with.
*/
bool CSFilter::operator==(const CSFilter& rhs) const
{
    return (m_nLayers == rhs.m_nLayers) &&
           (m_mode == rhs.m_mode) &&
           (m_mask == rhs.m_mask);
}

//! Tests if a TMarker is passed by the filter
/*!
\ingroup GpFilter
In AND mode, each marker code must have the corresponding bit set in the marker
mask. In OR mode, only mask 0 is used. One of the 4 marker codes MUST be present
in mask 0 with the exception that code 00 is only tested for the first marker
code.
\param mark A TMarker object that is to be filtered
\return true, the filter accepts the marker, false it does not
*/
bool CSFilter::Filter(const TMarker& mark) const
{
    switch(m_mode)
    {
    case eM_and:
        for (int i=0; i<m_nLayers; ++i)     // all codes must be in...
            if (!m_mask[i][mark.m_code[i]]) // ...all masks, else...
                return false;               // ...we fail
        return true;
        break;

    case eM_or:
        for (int i=0; i<m_nLayers; ++i)
        {
            uint8_t code = mark.m_code[i];
            if ( ((i==0) || code) && m_mask[0][code])   // layer 0 or non-zero code
                return true;                // and present means accept it
        }
        break;
    }
    return false;
}

//! Manipulate the filter
/*!
\ingroup GpFilter
With this command you can set, clear or invert either one or all items in
one or all layers.
\param layer Either the layer number (0..layers-1) or -1 for all layers
\param item Either the layer item number (0..255) or -1 for all items
\param action One of eS-clr, eS_set or eS_inv (0, 1, 2)
\return 0 if OK, -1 if a bad parameter
*/
int CSFilter::Control(int layer, int item, eSet action)
{
    if ((layer >= m_nLayers) || (item >= 256))
        return -1;

    size_t laylo = (layer<0) ? 0 : static_cast<size_t>(layer);
    size_t layhi = (layer<0) ? m_nLayers : laylo+1;
    for (auto layer = laylo; layer < layhi; ++layer)
    {
        TMask& bs(m_mask[layer]);
        if (item < 0)
        {
            switch(action)
            {
            case eS_clr: bs.reset(); break;
            case eS_set: bs.set(); break;
            case eS_inv: bs.flip(); break;
            }
        }
        else
        {
            const size_t n(static_cast<size_t>(item));
            switch(action)
            {
            case eS_clr: bs.reset(n); break;
            case eS_set: bs.set(n); break;
            case eS_inv: bs.flip(n); break;
            }
        }
    }
    m_active = eA_unset;    // active state unknown
    return 0;
}

//! Get the effect of the filter.
/*!
\ingroup GpFilter
This can be used to decide if there is no point filtering the data i.e. the
filter is passing all data (common), or it is passing no data (rare). It is
likely that you will have two loops for processing data: a (faster) loop with
no filtering and a (slower) one with filtering which has to allow for skipping
unwanted data. This routine lets you decide which to use.
There are three outcomes:

 return  | Meaning
 ------- | -------
 eA_none | The filter will never pass anything
 eA_all  | The filter will pass everything, equivalent to no filter
 eA_some | The filter will pass some things and not others

\param layer If -1 or >= layers, test all layers, else test a single layer. If you
             specify a layer, we ignore the mode. If you do not specify a layer, we
             only use layer 0 if we are in or mode.
\return The effect of applying the filter.
*/
CSFilter::eActive CSFilter::Active(int layer) const
{
    if (m_active == eA_unset)       // else we already know the result
    {
        if ((layer >=0) && (layer < m_nLayers))
        {
            if (m_mask[layer].all())
                return eA_all;
            else if (m_mask[layer].none())
                return eA_none;
            else
                return eA_some;
        }

        size_t n = m_mode == eM_and ? m_nLayers : 1;
        if (m_mask[0].all())
            m_active = eA_all;
        else if (m_mask[0].none())
            m_active = eA_none;
        else
            return m_active = eA_some;

        for (size_t i=1; i<n; ++i)
        {
            if (m_active == eA_all)
            {
                if (!m_mask[i].all())
                    return m_active = eA_some;
            }
            else
            {
                if (!m_mask[i].none())
                    return m_active = eA_some;
            }
        }
    }
    return m_active;
}

//! Return the state of an item in a layer
/*!
\ingroup GpFilter
\param layer If outside the range 0 to m_nLayers-1, return is false. This is
             the layer number to inspect.
\param item  The item number. if outside the range 0..255 the result is false.
\return true if the item is set, false if it is clear.
*/
bool CSFilter::GetItem(int layer, int item) const
{
    if (((unsigned)layer >= (unsigned)m_nLayers) ||
        ((unsigned)item >= TMask::NBit))
        return false;
    return m_mask[layer][item];
}

//! Get the TMask as a binary bit pattern
/*!
\ingroup GpFilter
This gives access to a TMask layer to allow it to be stored or converted to a
32-bit son file ceds32::TFilterMask layer.
\param pCopy Points at the target to be copied to, at least 32 bytes in size
\param layer The layer, 0-3 only
*/
void CSFilter::GetElements(void* pCopy, int layer) const
{
    if ((layer >= 0) && (layer <= 3) && pCopy)
    {
        memcpy(pCopy, &(m_mask[layer].m_mask[0]), TMask::NBit / 8);
    }
}

//! Set a TMask layer as a binary bit pattern
/*!
\ingroup GpFilter
This gives access to a TMask layer to allow it to be restored or converted from
a 32-bit son file ceds32::TFilterMask layer.
\param pCopy Points at the target to be copied from, at least 32 bytes in size
\param layer The layer, 0-3 only
*/
void CSFilter::SetElements(const void* pCopy, int layer)
{
    if ((layer >= 0) && (layer <= 3) && pCopy)
    {
        memcpy(&(m_mask[layer].m_mask[0]), pCopy, TMask::NBit / 8);
    }
}

//! Set the column to return when reading extended marker data
/*!
\ingroup GpFilter
This is intended for the case where we read from AdcMark channels with multiple
traces and we want to specify the trace to be used. However, it could be used with
any extended marker that has more than 1 column.
\param nCol The column to use. The first is 0. Setting a column that does not exist
            will be equivalent to column 0.
*/
void CSFilter::SetColumn(int nCol)
{
    if (nCol >= -1)
        m_nColumn = nCol;
}

//! Get the column to return when reading extended marker data
/*!
\ingroup GpFilter
This is intended for the case where we read from AdcMark channels with multiple
traces and we want to know the trace to be used. However, it could be used with
any extended marker that has more than 1 column.
\return The 0-based column number.
*/
int CSFilter::GetColumn() const
{
    return m_nColumn;
}

#ifdef TESTME
int testFilter()
{
    int nErrors = 0;
    CSFilter f;                     // make a filter
    if (f.Active() != CSFilter::eA_all)
        ++nErrors;
    f.Control(0, -1, CSFilter::eS_clr);
    if (f.Active() != CSFilter::eA_some)
        ++nErrors;
    f.SetMode(CSFilter::eM_or);
    if (f.Active() != CSFilter::eA_none)
        ++nErrors;
    return nErrors;
}
#endif