// This is a generic plugin that does nothing useful apart from acting as an example... of a plugin that does nothing :D

// some headers required for a plugin. Nothing special, just the basics.
#include "Core.h"
#include <Console.h>
#include <Export.h>
#include <PluginManager.h>

// DF data structure definition headers
#include "df/api.h"
//#include "df/world.h"

using namespace DFHack;
using namespace df::enums;

// our own, empty header.
#include "skeleton.h"


// Here go all the command declarations...
// mostly to allow having the mandatory stuff on top of the file and commands on the bottom
command_result skeleton (Core * c, std::vector <std::string> & parameters);

// A plugins must be able to return its name. This must correspond to the filename - skeleton.plug.so or skeleton.plug.dll
DFhackCExport const char * plugin_name ( void )
{
    return "skeleton";
}

// Mandatory init function. If you have some global state, create it here.
DFhackCExport command_result plugin_init ( Core * c, std::vector <PluginCommand> &commands)
{
    // Fill the command list with your commands.
    commands.clear();
    commands.push_back(PluginCommand(
        "skeleton", "Do nothing, look pretty.",
        skeleton, false, /* true means that the command can't be used from non-interactive user interface */
        // Extended help string. Used by CR_WRONG_USAGE and the help command:
        "  This command does nothing at all.\n"
        "Example:\n"
        "  skeleton\n"
        "    Does nothing.\n"
    ));
    return CR_OK;
}

// This is called right before the plugin library is removed from memory.
DFhackCExport command_result plugin_shutdown ( Core * c )
{
    // You *MUST* kill all threads you created before this returns.
    // If everythin fails, just return CR_FAILURE. Your plugin will be
    // in a zombie state, but things won't crash.
    return CR_OK;
}

// Called to notify the plugin about important state changes.
// Invoked with DF suspended, and always before the matching plugin_onupdate.
// More event codes may be added in the future.
/*
DFhackCExport command_result plugin_onstatechange(Core* c, state_change_event event)
{
    switch (event) {
    case SC_GAME_LOADED:
        // initialize from the world just loaded
        break;
    case SC_GAME_UNLOADED:
        // cleanup
        break;
    default:
        break;
    }
    return CR_OK;
}
*/

// Whatever you put here will be done in each game step. Don't abuse it.
// It's optional, so you can just comment it out like this if you don't need it.
/*
DFhackCExport command_result plugin_onupdate ( Core * c )
{
    // whetever. You don't need to suspend DF execution here.
    return CR_OK;
}
*/

// A command! It sits around and looks pretty. And it's nice and friendly.
command_result skeleton (Core * c, std::vector <std::string> & parameters)
{
    // It's nice to print a help message you get invalid options
    // from the user instead of just acting strange.
    // This can be achieved by adding the extended help string to the
    // PluginCommand registration as show above, and then returning
    // CR_WRONG_USAGE from the function. The same string will also
    // be used by 'help your-command'.
    if (!parameters.empty())
        return CR_WRONG_USAGE;
    // Commands are called from threads other than the DF one.
    // Suspend this thread until DF has time for us. If you
    // use CoreSuspender, it'll automatically resume DF when
    // execution leaves the current scope.
    CoreSuspender suspend(c);
    // Actually do something here. Yay.
    c->con.print("Hello! I do nothing, remember?\n");
    // Give control back to DF.
    return CR_OK;
}
