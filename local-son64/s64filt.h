// s64filt.h
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

#ifndef __S64FILT_H__
#define __S64FILT_H__
#include <array>

//! \file
//! \brief Interface to the marker filtering system of the SON64 filing system

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
    struct TMarker;

    /*! \defgroup GpFilter CSFilter and member functions
    \brief The CSFilter class used to filter marker and extended marker data.

     The CSFilter class is used to filter markers and extended marker data types.
     Functions that can read data of these types have an argument that can be set
     to `nullptr` meaning accept everything, or can be set to point at a filter to
     use to limit the returned items to those that match the filter.
    */

    //! Single layer of the bitmap that indicates which codes are included
    /*!
    \ingroup GpFilter
    A CSFilter has space for 8 of these maps, though currently only the first 4 maps
    are used. The map has one bit per element. We provide routines to set, clear and
    invert one or all bits in the mask.
    */
    struct TMask
    {
        enum
        {
            NBit = 256,                 //!< bits per layer
            NPer = sizeof(uint32_t)*8,  //!< bits per unsigned int used for storage
            NItem = NBit / NPer,        //!< Number of unsigned int per layer
            IndexMask = NPer-1,         //!< Mask to extract bit# from code
            IndexShift = 5              //!< Shift to get unsigned in index
        };
        static_assert(NPer == (1 << IndexShift), "TMask constants bad");
        std::array<uint32_t, NItem> m_mask;  //!< Array of bits making up a mask

        void set();                     //!< Set all mask bits
        void set(size_t n);             //!< Set a particular bit
        void reset();                   //!< clear all mask bits
        void reset(size_t n);           //!< clear a particular bit
        void flip();                    //!< invert entire mask
        void flip(size_t n);            //!< invert a particular bit
        bool none() const;              //!< true if no bits set in mask
        bool all() const;               //!< true if all bits set in mask
        bool test(size_t n) const;      //!< true if bit n is set
        bool operator[](size_t n) const;//!< return state of bit n

        //! True if passed in mask is identical
        bool operator==(const TMask& rhs) const {return m_mask == rhs.m_mask;}
    };

    //! Class to handle filtering of TMarker data
    /*!
    \ingroup GpFilter
    Objects of this class are used to filter Marker-derived channels to determine if
    a particular data item is wanted or not. This class is also used to choose which
    trace AdcMark channels with multiple traces return when read as Adc data.
    */
    class CSFilter
    {
    public:
        DllClass CSFilter();

        //! Enumerate the filter mode
        enum eMode
        {
            eM_and = 0,     //!< All codes must match
            eM_or           //!< Any code may match
        };
        eMode DllClass GetMode() const;
        void DllClass SetMode(eMode mode);
        bool DllClass Filter(const TMarker& mark) const;

        //! Enumerate settings for the Control() command
        enum eSet
        {
            eS_clr=0,       //!< Clear the items from the filter
            eS_set,         //!< Set the items, add to the filter
            eS_inv          //!< Invert selected items state in the filter
        };
        int DllClass Control(int layer, int item, eSet action);
        bool DllClass operator==(const CSFilter& rhs) const;

        //! Enumerator to describe the state of the filter
        enum eActive
        {
            eA_unset = 0,   //!< We do not know the filter state
            eA_none,        //!< This filter will pass no data
            eA_some,        //!< This filter will pass some data, so must be used.
            eA_all          //!< This filter passes all data, so can be ignored.
        };
        eActive DllClass Active(int layer = -1) const;
        bool DllClass GetItem(int layer, int item) const;
        void DllClass SetColumn(int nCol);
        int DllClass GetColumn() const;

        void DllClass GetElements(void* pCopy, int layer) const;
        void DllClass SetElements(const void* pCopy, int layer);

    private:
        std::array<TMask, 8> m_mask;
        int m_nLayers;                  //!< The number of layers in use (4 or 8).
        int m_nColumn;                  //!< -1 for all, else trace number to use with AdcMark
        eMode m_mode;                   //!< The mode of the marker filter
        mutable eActive m_active;       //!< Used internally to reduce filtering time
    };
}
#undef DllClass
#endif