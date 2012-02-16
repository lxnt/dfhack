#include "Core.h"
#include "Console.h"
#include "Export.h"
#include "PluginManager.h"

#include "df/api.h"
#include "df/d_init.h"

using std::vector;
using std::string;
using std::endl;
using namespace DFHack;
using namespace df::enums;

using df::global::d_init;

command_result twaterlvl(Core * c, vector <string> & parameters);
command_result tidlers(Core * c, vector <string> & parameters);

DFhackCExport const char * plugin_name ( void )
{
    return "initflags";
}

DFhackCExport command_result plugin_init (Core *c, std::vector <PluginCommand> &commands)
{
    commands.clear();
    if (d_init) {
        commands.push_back(PluginCommand("twaterlvl", "Toggle display of water/magma depth.",
                                         twaterlvl, dwarfmode_hotkey));
        commands.push_back(PluginCommand("tidlers", "Toggle display of idlers.",
                                         tidlers, dwarfmode_hotkey));
    }
    std::cerr << "d_init: " << sizeof(df::d_init) << endl;
    return CR_OK;
}

DFhackCExport command_result plugin_shutdown ( Core * c )
{
    return CR_OK;
}

command_result twaterlvl(Core * c, vector <string> & parameters)
{
    // HOTKEY COMMAND: CORE ALREADY SUSPENDED
    d_init->flags1.toggle(d_init_flags1::SHOW_FLOW_AMOUNTS);
    c->con << "Toggled the display of water/magma depth." << endl;
    return CR_OK;
}

command_result tidlers(Core * c, vector <string> & parameters)
{
    // HOTKEY COMMAND: CORE ALREADY SUSPENDED
    d_init->idlers = ENUM_NEXT_ITEM(d_init_idlers, d_init->idlers);
    c->con << "Toggled the display of idlers to " << ENUM_KEY_STR(d_init_idlers, d_init->idlers) << endl;
    return CR_OK;
}
