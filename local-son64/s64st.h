// s64st.h
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

#ifndef __S64ST_H__
#define __S64ST_H__
//! \file s64st.h
//! \brief Header of the CSaveTimes class used in the circular buffers
//! \internal

#include "s64.h"
#include <vector>

namespace ceds64
{
    /*!
	 * This constant limits the number of state changes we accumulate that make no difference
     * to the data we save on disk. We just dump the oldest changes in the circular buffers
     * that have no associated data. This can make a difference to what is displayed by events
     * drawn as lines, so we allow quite a lot before we set a limit.
	*/
    enum eSaveTimes {eST_MaxDeadEvents = 100};

	//! Class to manage the list of save/no save times and the save state
    class CSaveTimes
    {
        std::vector<TSTime64> m_vTimes; //!< change of save/nosave times list
        ceds64::TSTime64 m_tStart;      //!< First time we know about save state
        bool m_bStartSaved;             //!< True if saving state at m_tStart else false
        mutable size_t m_nFetch;        //!< next index for NextSaveRange()

    public:
        //! Enumerates the state of saving
        enum eST
        {
            eST_none = 0,               //!< Nothing is being saved (list is empty_
            eST_all = 1,                //!< Everything is being saved (list is empty)
            eST_some = 2                //!< some items saved and some not saved (list not empty)
        };

        CSaveTimes()
            : m_tStart( 0 )
            , m_bStartSaved( true )
            , m_nFetch( 0 )
        {}

        void Reset();                   //! Set back to initial state

        eST State() const;
        void SetSave(ceds64::TSTime64 t, bool bSave);
        void SaveRange(ceds64::TSTime64 tFrom, ceds64::TSTime64 tUpto);
        void SetFirstTime(ceds64::TSTime64 t);
        void SetDeadRange(ceds64::TSTime64 tLastData, ceds64::TSTime64 tReached, int nKeep = 0);
        bool FirstSaveRange(ceds64::TSTime64* pFrom, ceds64::TSTime64* pUpto, ceds64::TSTime64 tUpto, ceds64::TSTime64 tFrom = -1) const;
        bool NextSaveRange(ceds64::TSTime64* pFrom, ceds64::TSTime64* pUpto, ceds64::TSTime64 tUpto) const;
        bool IsSaving(ceds64::TSTime64 tAt) const;
        int  NoSaveList(ceds64::TSTime64* pTimes, int nMax, ceds64::TSTime64 tFrom = -1, ceds64::TSTime64 tUpto = TSTIME64_MAX) const;
     };
}
#endif