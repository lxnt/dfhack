/*
https://github.com/peterix/dfhack
Copyright (c) 2009-2011 Petr Mrázek (peterix@gmail.com)

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must
not claim that you wrote the original software. If you use this
software in a product, an acknowledgment in the product documentation
would be appreciated but is not required.

2. Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any source
distribution.
*/

#pragma once
#include "Export.h"
#include <iostream>
#include <iomanip>
#include <climits>
#include <stdint.h>
#include <vector>
#include <sstream>
#include <cstdio>

using namespace std;

template <typename T>
void print_bits ( T val, DFHack::Console& out )
{
    stringstream strs;
    T n_bits = sizeof ( val ) * CHAR_BIT;
    int cnt;
    for ( unsigned i = 0; i < n_bits; ++i )
    {
        cnt = i/10;
        strs << cnt << " ";
    }
    strs << endl;
    for ( unsigned i = 0; i < n_bits; ++i )
    {
        cnt = i%10;
        strs << cnt << " ";
    }
    strs << endl;
    for ( unsigned i = 0; i < n_bits; ++i )
    {
        strs << "--";
    }
    strs << endl;
    for ( unsigned i = 0; i < n_bits; ++i )
    {
        strs<< !!( val & 1 ) << " ";
        val >>= 1;
    }
    strs << endl;
    out.print(strs.str().c_str());
}

/*
 * List
 */

template<typename Link>
Link *linked_list_append(Link *head, Link *tail)
{
    while (head->next)
        head = head->next;
    head->next = tail;
    tail->prev = head;
    return tail;
}

template<typename Link>
Link *linked_list_insert_after(Link *pos, Link *link)
{
    link->next = pos->next;
    if (pos->next)
        pos->next->prev = link;
    link->prev = pos;
    pos->next = link;
    return link;
}

/*
 * MISC
 */

DFHACK_EXPORT bool split_string(std::vector<std::string> *out,
                                const std::string &str, const std::string &separator,
                                bool squash_empty = false);

DFHACK_EXPORT std::string toUpper(const std::string &str);
DFHACK_EXPORT std::string toLower(const std::string &str);

inline bool bits_match(unsigned required, unsigned ok, unsigned mask)
{
    return (required & mask) == (required & mask & ok);
}

template<typename T, typename T1, typename T2>
inline T clip_range(T a, T1 minv, T2 maxv) {
    if (a < minv) return minv;
    if (a > maxv) return maxv;
    return a;
}

/**
 * Returns the amount of milliseconds elapsed since the UNIX epoch.
 * Works on both windows and linux.
 * source: http://stackoverflow.com/questions/1861294/how-to-calculate-execution-time-of-a-code-snippet-in-c
 */
DFHACK_EXPORT uint64_t GetTimeMs64();

DFHACK_EXPORT std::string stl_sprintf(const char *fmt, ...);
DFHACK_EXPORT std::string stl_vsprintf(const char *fmt, va_list args);
