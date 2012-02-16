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
#ifndef CL_MOD_GUI
#define CL_MOD_GUI

#include "Export.h"
#include "Module.h"
#include "Virtual.h"
#include <string>

#include "df/api.h"
#include "df/init.h"
#include "df/ui.h"

namespace df {
    struct viewscreen;
    struct job;
    struct unit;
    struct item;
};

/**
 * \defgroup grp_gui query DF's GUI state
 * @ingroup grp_modules
 */

namespace DFHack
{
    class Core;

    // Full-screen item details view
    DFHACK_EXPORT bool item_details_hotkey(Core *, df::viewscreen *top);
    // 'u'nits or 'j'obs full-screen view
    DFHACK_EXPORT bool unitjobs_hotkey(Core *, df::viewscreen *top);

    // A job is selected in a workshop
    DFHACK_EXPORT bool workshop_job_hotkey(Core *c, df::viewscreen *top);
    // Building material selection mode
    DFHACK_EXPORT bool build_selector_hotkey(Core *c, df::viewscreen *top);
    // A unit is selected in the 'v' mode
    DFHACK_EXPORT bool view_unit_hotkey(Core *c, df::viewscreen *top);
    // Above + the inventory page is selected.
    DFHACK_EXPORT bool unit_inventory_hotkey(Core *c, df::viewscreen *top);

    // In workshop_job_hotkey, returns the job
    DFHACK_EXPORT df::job *getSelectedWorkshopJob(Core *c, bool quiet = false);

    // A job is selected in a workshop, or unitjobs
    DFHACK_EXPORT bool any_job_hotkey(Core *c, df::viewscreen *top);
    DFHACK_EXPORT df::job *getSelectedJob(Core *c, bool quiet = false);

    // A unit is selected via 'v', 'k', unitjobs, or
    // a full-screen item view of a cage or suchlike
    DFHACK_EXPORT bool any_unit_hotkey(Core *c, df::viewscreen *top);
    DFHACK_EXPORT df::unit *getSelectedUnit(Core *c, bool quiet = false);

    // An item is selected via 'v'->inventory, 'k', 't', or
    // a full-screen item view of a container. Note that in the
    // last case, the highlighted contained item is returned, not
    // the container itself.
    DFHACK_EXPORT bool any_item_hotkey(Core *c, df::viewscreen *top);
    DFHACK_EXPORT df::item *getSelectedItem(Core *c, bool quiet = false);

    // Show a plain announcement, or a titan-style popup message
    DFHACK_EXPORT void showAnnouncement(std::string message, int color = 7, bool bright = true);
    DFHACK_EXPORT void showPopupAnnouncement(std::string message, int color = 7, bool bright = true);

    class DFContextShared;

    /**
     * One tile of the screen. Possibly outdated.
     * \ingroup grp_gui
     */
    struct t_screen
    {
        uint8_t symbol;
        uint8_t foreground;
        uint8_t background;
        uint8_t bright;
        uint8_t gtile;
        uint8_t grayscale;
    };

    /**
     * The Gui module
     * \ingroup grp_modules
     * \ingroup grp_gui
     */
    class DFHACK_EXPORT Gui: public Module
    {
        public:

        Gui();
        ~Gui();
        bool Start();
        bool Finish();
        /*
         * Cursor and window coords
         */
        bool getViewCoords (int32_t &x, int32_t &y, int32_t &z);
        bool setViewCoords (const int32_t x, const int32_t y, const int32_t z);
        
        bool getCursorCoords (int32_t &x, int32_t &y, int32_t &z);
        bool setCursorCoords (const int32_t x, const int32_t y, const int32_t z);

        bool getDesignationCoords (int32_t &x, int32_t &y, int32_t &z);
        bool setDesignationCoords (const int32_t x, const int32_t y, const int32_t z);

        bool getMousePos (int32_t & x, int32_t & y);
        /*
         * Gui screens
         */
        /// Get the current top-level view-screen
        df::viewscreen * GetCurrentScreen();
        /// The DF menu state (designation menu ect)
        uint32_t * df_menu_state;

        /*
         * Window size in tiles
         */
        bool getWindowSize(int32_t & width, int32_t & height);
        
        /*
         * Screen tiles
         */
        bool getScreenTiles(int32_t width, int32_t height, t_screen screen[]);

        private:
        struct Private;
        Private *d;
    };
}
#endif

