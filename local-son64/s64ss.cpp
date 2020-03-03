// son64ss.cpp
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

//! \file s64ss.cpp
//! \brief string store implementation for the SON64 library

#include "s64priv.h"

// NBNB: In a multi-threaded environment you must be holding the string store
//       mutex to call any routine in the string store.

//=============================== string store ===============================
// We do not store fixed length strings as part of the structures saved on disk.
// Instead, we store a string index, which is an index into a string table. The
// string table lets you lookup a string as either an index or by the text. We
// store a list of strings and a reference count for each. We attach the string
// table to the end of the file header stream of disk blocks.
//
// The string table always holds an empty string, which is at index 0 and always
// has a reference count of 1.

using namespace ceds64;
using namespace std;

//! Set an entry based on a pointer to a string
/*!
This is used when restoring the string from a memory image.
\param pSrc Points at a string to store in this object.
\param nRef The number of references to the string
\return     The number of uint32_t items needed to hold the string and its zero
            padding when in the table.
*/
uint32_t ref_string::SetFromStr(const char* pSrc, uint32_t nRef)
{
    assert(nRef && pSrc && *pSrc);          // sanity check
    m_n = nRef;                             // set the table entries
    m_s = pSrc;                             // copy to the string
    size_t n = m_s.size();                  // size of the string
    return static_cast<uint32_t>((n + sizeof(uint32_t))/sizeof(uint32_t));
}

//! Is this entry a valid string
/*!
To be valid, we need a non-zero reference count and the string must not have 0 length.
\return     True If the string is not of zero length and has a valid number of references.
*/
bool ceds64::ref_string::IsValid() const
{
    return m_n && !m_s.empty();
}

//! Return a string based on the id
/*!
\param n    Either 0 (an empty string) or the string id to retrieve. If this id
            does not exist we return an empty string.
\return     The string from the store or an empty string if not found.
*/
string string_store::String(s64strid n) const
{
    if (n == 0)
        return string();
    auto it = m_bimap.right.find(n);        // see if string is here
    if (it == m_bimap.right.end())          // if not found
        return string();                    // this is an error?
    return (*it).second.String();           // get the string
}

//! Remove a string reference from the store
/*!
If a string with the given reference exists in the store, reduce the reference
count by 1. If it becomes 0, delete the string from the store and save the index
in the free list. The m_bModified flag is set if a change is made.
\param n The id of a string.
*/
void string_store::Sub(s64strid n)
{
    if (n == 0)                             // never do anything with item 0
        return;                             // as this is the blank string
    auto it = m_bimap.right.find(n);        // see if string is here
    if (it == m_bimap.right.end())          // if not found
        return;                             // this may be an error?
    if (!(*it).second.DecRef())             // if decrement ref makes it unused...
    {
        if (m_bimap.size() == 1)            // if this is the last item
            clear();                        // erase everything
        else
        {
            auto itTest(it);
            if (++itTest == m_bimap.right.end())  // if this is the last active index
            {
                itTest = it;                    // We can reduce the table size to the last used item
                s64strid prev = (--itTest)->first;  // there MUST be a previous as at least 2
                auto fit = lower_bound(m_free.begin(), m_free.end(), prev+1);
                m_free.erase(fit, m_free.end()); // remove all higher items
            }
            else                                // Not the last, so...
                m_free.insert(n);               // ...save the index in the free list
            m_bimap.right.erase(it);            // remove the entry
        }
    }
    SetModified();                          // we have made a change
}

//! Add the given string to the store.
/*!
If it is already in the store, increase the reference count by 1, otherwise add
a new element with a ref count of 1. m_bModified is set if a change is made.
\param s   The new string to add. If blank, only the old string is removed.
\param old The string ID of the old string to remove or 0 if none. You use this when
           modifying a string, so old is the original string ID.
\return    The index of the added string. If the old string is the same, then the
           store is not changed.
*/
s64strid string_store::Add(const string& s, s64strid old)
{
    if (s.empty())                          // new string is blank...
    {
        Sub(old);                           // Remove old (unless 0)
        return 0;                           // and we are done
    }

    // If we add the same string we are removing there is no change...
    s64strid index;
    ref_string rs(s);                       // make a suitable object
    auto it = m_bimap.left.find(rs);        // do we already exist?
    if (it == m_bimap.left.end())           // does not exist
    {
        if (!m_free.empty())
        {
            auto it = m_free.begin();       // first item
            index = *it;                    // get the first free index (lowest number)
            m_free.erase(it);               // and remove it from the free list
        }
        else
            index = (s64strid)m_bimap.size()+1;   // next index (0 is reserved for empty string)
        m_bimap.insert(strbimap::value_type(rs, index));
        Sub(old);                           // remove the old one (unless 0)
        SetModified();                      // modified the string store
    }
    else                                    // string already in the table
    {
        index = it->second;
        if (index != old)                   // if not 
        {
            it->first.IncRef();
            SetModified();
        }
    }
    return index;
}

//! Generate a vector that holds the string store in the disk format.
/*!
To do this you must have exclusive ownership of the string store.
The disk format is based on uint32_t objects (even though some of them are used
to hold text strings). We rely on the fact that a vector is contiguous memory.

index   | Contents
--------|:---------
0       | The total number of uint32_t values in the image
1       | The number of index entries written to the image
2       | 0 for unused index entry, or the reference count
3..     | little endian 0-terminated string padded to a 4 byte boundary.
...     | further entries until we reach the highest index value saved.

So a string of n bytes will occupy 1 uint32_t for the reference count, then a further
(n+4)/4 uint32_t items for the string. An unused entry occupies 1 uint32_t value of 0.

\param pImage Call with this set to nullptr to get the number of uint32_t elements to
              allocate to hold the image. If this is set, the first element should be
              passed in set to the number of elements.
\return       The number of uint32_t items occupied by the string table, or 0 for an
              error. The minimum valid return is 2 for an empty table holding 0,0.
*/
uint32_t string_store::BuildImage(uint32_t* pImage) const
{
    s64strid nextIndex = 1;                 // the index we expect next
    auto iit = m_bimap.right.begin();       // we will iterate by index
    auto fit = m_free.cbegin();             // free list iterator (for checking)
    if (pImage == nullptr)                  // we are just counting the size
    {
        size_t n = 2;                     // Allow space for the table size and index size
        while (iit != m_bimap.right.end())
        {
            n += 1;                         // one element used for the ref count or 0
            s64strid id = iit->first;       // this is the index
            if (id > nextIndex)             // we have a missing item
            {
                if (fit == m_free.cend())   // verify that free list matches
                    return 0;
                else
                {
                    assert(*fit == nextIndex);
                    ++fit;
                }
            }
            else if (id == nextIndex)       // we have the string
            {
                // Calculate the space needed for the string.
                size_t nLen = iit->second.StringSize();
                if ((iit->second.RefCount() == 0) || (nLen == 0))
                    return 0;               // something is badly wrong
                n += (nLen + sizeof(uint32_t)) / sizeof(uint32_t);
                ++iit;                      // on to the next item
            }
            else
                return 0;                   // something logically wrong
            ++nextIndex;
       }
        return (fit == m_free.cend()) ? static_cast<uint32_t>(n) : 0;
    }
    else                                    // we are filling in the table
    {
        uint32_t *pNext = pImage+2;         // skip over the header
        while (iit != m_bimap.right.end())
        {
            s64strid id = iit->first;       // this is the index
            if (id > nextIndex)             // we have a missing item
                *pNext++ = 0;               // an unused index entry
            else if (id == nextIndex)       // we have the string
            {
                // Calculate the space needed for the string. Use of memcpy is nasty, but we have little
                // choice and using strcpy() generates copious warnings and is slower. The pNext[nInc]
                // is correct; there is an extra word at the front used to save the ref count.
                size_t n = iit->second.StringSize();    // bytes to copy
                size_t nInc = (n + sizeof(uint32_t)) / sizeof(uint32_t); // space in uint32_t units
                pNext[nInc] = 0;                    // make sure any unused space after string holds 0
                *pNext++ = iit->second.RefCount();  // fill in the reference count
                const string& s = iit->second.String();
                memcpy(reinterpret_cast<char*>(pNext), s.c_str(), n+1);
                pNext += nInc;              // skip over the string and terminating 0's
                ++iit;                      // on to the next item
            }
            ++nextIndex;
        }

        uint32_t n = static_cast<uint32_t>(pNext-pImage);
        pImage[1] = nextIndex - 1;          // number of table entries
        assert(pImage[0] == n);             // assert sizes match
        return n;
    }
}

//! Load a string_store from a memory images saved by BuildImage
/*!
The m_bModified flag is set to false as after this, the store is presumed to
match what was on disk as well as it ever will. We will try to survive, even if the
string store appears to be in a mess.
\param pImage           See BuildImage() for the string format.
\param nMaxReferences   Either 0, or the maximum possible number of references to any string.
                        This is for verification purposes.
\return                 True if we seem to have loaded an image correctly.     
*/
bool string_store::LoadFromImage(uint32_t* pImage, uint32_t nMaxReferences)
{
    clear();                                // empty the string store
    uint32_t* pWork = pImage;               // a copy to increment
    uint32_t nSize = *pWork++;              // Size of image in uint32_t items
    uint32_t nEntry = *pWork++;             // number of entries

    if (nSize < 2 + 2 * nEntry)             // Minimum entry size is 2 uint32_t items
        return false;

    uint32_t* pLimit = pWork + nSize - 2;   // point past used section (-2 for 2 x ++)
    ref_string rs;                          // storage for each string
    if (nMaxReferences == 0)                // if no limit set...
        nMaxReferences = UINT32_MAX;        // ...then accept anything
    for (s64strid index = 1; index <= nEntry; ++index)
    {
        if (pWork >= pLimit)                // something is badly wrong            
            return false;

        uint32_t nRefCount = *pWork++;      // 0= unused or ref count for used
        if (nRefCount)
            pWork += rs.SetFromStr(reinterpret_cast<char*>(pWork), nRefCount);

        if (nRefCount && (nRefCount <= nMaxReferences) && rs.IsValid())
            m_bimap.insert(strbimap::value_type(rs, index));
        else
        {
            m_free.insert(index);           // add to free list
            assert(index < nEntry);         // last entry should not be free
        }
    }
    assert(m_free.size()+m_bimap.size() == nEntry);
    SetModified(false);
    return pWork == pLimit;
}

//! Check that passed in id will get a valid string
/*!
All empty strings have an id of 0, so 0 is always valid. Otherwise, a valid id
will have a non-empty string in the store.
\param id   Identifies a string in the store.
\return     true if using this id will get a valid string (may be empty).
*/
bool string_store::Verify(s64strid id) const
{
    return (id == 0) || !String(id).empty();
}