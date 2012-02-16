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
/*
* Items!
*/
#include "Export.h"
#include "Module.h"
#include "Types.h"
#include "Virtual.h"
#include "modules/Materials.h"
#include "MemAccess.h"

#include "df/api.h"
#include "df/item.h"
#include "df/item_type.h"
#include "df/general_ref.h"

namespace df
{
    struct itemdef;
}

/**
 * \defgroup grp_items Items module and its types
 * @ingroup grp_modules
 */

namespace DFHack
{
    struct DFHACK_EXPORT ItemTypeInfo {
        df::item_type type;
        int16_t subtype;

        df::itemdef *custom;

    public:
        ItemTypeInfo(df::item_type type_ = df::enums::item_type::NONE, int16_t subtype_ = -1) {
            decode(type_, subtype_);
        }
        template<class T> ItemTypeInfo(T *ptr) { decode(ptr); }

        bool isValid() const {
            return (type != df::enums::item_type::NONE) && (subtype == -1 || custom);
        }

        bool decode(df::item_type type_, int16_t subtype_ = -1);
        bool decode(df::item *ptr);

        template<class T> bool decode(T *ptr) {
            return ptr ? decode(ptr->item_type, ptr->item_subtype) : decode(df::enums::item_type::NONE);
        }

        std::string getToken();
        std::string toString();

        bool find(const std::string &token);

        bool matches(const df::job_item &item, MaterialInfo *mat = NULL);
    };

    inline bool operator== (const ItemTypeInfo &a, const ItemTypeInfo &b) {
        return a.type == b.type && a.subtype == b.subtype;
    }
    inline bool operator!= (const ItemTypeInfo &a, const ItemTypeInfo &b) {
        return a.type != b.type || a.subtype != b.subtype;
    }

/**
 * Type for holding an item read from DF
 * \ingroup grp_items
 */
struct dfh_item
{
    df::item *origin; // where this was read from
    int16_t x;
    int16_t y;
    int16_t z;
    df::item_flags flags;
    uint32_t age;
    uint32_t id;
    t_material matdesc;
    int32_t quantity;
    int32_t quality;
    int16_t wear_level;
};

/**
 * The Items module
 * \ingroup grp_modules
 * \ingroup grp_items
 */
namespace Simple
{
namespace Items
{

/// Look for a particular item by ID
DFHACK_EXPORT df::item * findItemByID(int32_t id);

/// Make a partial copy of a DF item
DFHACK_EXPORT bool copyItem(df::item * source, dfh_item & target);
/// write copied item back to its origin
DFHACK_EXPORT bool writeItem(const dfh_item & item);

/// get the class name of an item
DFHACK_EXPORT std::string getItemClass(const df::item * item);
/// who owns this item we already read?
DFHACK_EXPORT int32_t getItemOwnerID(const df::item * item);
DFHACK_EXPORT df::unit *getItemOwner(const df::item * item);
/// which item is it contained in?
DFHACK_EXPORT int32_t getItemContainerID(const df::item * item);
DFHACK_EXPORT df::item *getItemContainer(const df::item * item);
/// which items does it contain?
DFHACK_EXPORT bool getContainedItems(const df::item * item, /*output*/ std::vector<int32_t> &items);
/// wipe out the owner records
DFHACK_EXPORT bool removeItemOwner(df::item * item);
/// read item references, filtered by class
DFHACK_EXPORT bool readItemRefs(const df::item * item, const df::general_ref_type type,
                  /*output*/ std::vector<int32_t> &values);
}
}
}