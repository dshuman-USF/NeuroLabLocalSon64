// son64ss.h
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

#ifndef __S64SS_H__
#define __S64SS_H__
//! \file s64ss.h
//! \brief String store definitions for the 64-bit son file system
//! \internal

#include <cstdint>
#include <string>
#include <set>
#include <boost/bimap.hpp>

namespace ceds64
{
	//! Class to handle a string and the number of references to it that exist
    class ref_string
    {
        std::string m_s;        //!< our string object
        mutable uint32_t m_n;   //!< number of references, does not affect map
    public:
        ref_string() : m_n( 1 ) {}; //!< creates an enpty string with ref count of 1

        //! Construct a new ref_string
        /*!
        \param rhs The string to create a ref_string for
        \param n    The number of references to it
        */
        ref_string(const std::string& rhs, uint32_t n=1)
            : m_s(rhs)
            , m_n( n )
        {}

        // We accept the compiler generated copy constructor and operator=()

        //! Comparison operator to allow us to search
        /*!
        \param rhs The string to compare with the one we hold.
        */
        bool operator<(const ref_string& rhs) const
        {
           return m_s < rhs.m_s;
        }

        void IncRef() const {++m_n;}            //!< Increase the references to this string
        bool DecRef() const {return --m_n > 0;} //!< reduce references, return false if bad count

        //! Set the entire contents of the ref_string, replacing current contents
        /*!
        \param s    The string to store
        \param n    The number of references to it
        */
        void SetString(const std::string& s, uint32_t n = 1)
        {
            m_s=s; m_n=n;
        }

        const std::string& String() const {return m_s;} //!< Access the string
        size_t StringSize() const {return m_s.size();}  //!< Access the string length
        uint32_t RefCount() const {return m_n;}         //!< Access the reference count
        uint32_t SetFromStr(const char* pSrc, uint32_t nRef);
        bool IsValid() const;
    };

    typedef uint32_t s64strid;      //!< type for ident of a string in the file
    typedef boost::bimap<ref_string, s64strid> strbimap; //!< bidirectional map<ref_string, s64strid>
    typedef std::set<uint32_t> uset; //!< Set of unused index values in the store

	//! Class to handle the string store when held in memory
    class string_store
    {
        strbimap m_bimap;           //!< The bi-directional map from ref_string to the id
        uset m_free;                //!< The set of free index values
        bool m_bModified;           //!< True if the store is modified, so needs writing
 
    public:
        string_store() : m_bModified( false ) {}
        s64strid Add(const std::string& s, s64strid old = 0);
        void Sub(s64strid n);

        //! Empty the string store
        /*!
        Note that this does not change the m_bModified variable. It empties the
        map of index to strings and the free index map.
        */
        void clear()
        {
            m_bimap.left.clear(); 
            m_free.clear();
        }

        std::string String(s64strid n) const;
        uint32_t BuildImage(uint32_t* pImage) const;
        bool LoadFromImage(uint32_t* pImage, uint32_t nMaxReferences = 0);

        //! Set or clear the modified flag
        /*!
        \param bModified The new modified state
        */
        void SetModified(bool bModified=true)
        {
            m_bModified = bModified;
        }
        bool IsModified() const {return m_bModified;}   //!< Does the string store need writing?
        bool Verify(s64strid id) const;
    };
}
#endif