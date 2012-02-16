// (un)designate matching plants for gathering/cutting

#include "Core.h"
#include "Console.h"
#include "Export.h"
#include "PluginManager.h"

#include "df/api.h"
#include "TileTypes.h"
#include "df/world.h"
#include "df/map_block.h"
#include "df/tile_dig_designation.h"
#include "df/plant_raw.h"

#include "modules/Vegetation.h"
#include <set>

using std::string;
using std::vector;
using std::set;
using namespace DFHack;
using namespace df::enums;

using df::global::world;

command_result df_getplants (Core * c, vector <string> & parameters)
{
    string plantMatStr = "";
    set<int> plantIDs;
    set<string> plantNames;
    bool deselect = false, exclude = false, treesonly = false, shrubsonly = false;

    int count = 0;
    for (size_t i = 0; i < parameters.size(); i++)
    {
        if(parameters[i] == "help" || parameters[i] == "?")
            return CR_WRONG_USAGE;
        else if(parameters[i] == "-t")
            treesonly = true;
        else if(parameters[i] == "-s")
            shrubsonly = true;
        else if(parameters[i] == "-c")
            deselect = true;
        else if(parameters[i] == "-x")
            exclude = true;
        else
            plantNames.insert(parameters[i]);
    }
    if (treesonly && shrubsonly)
    {
        c->con.printerr("Cannot specify both -t and -s at the same time!\n");
        return CR_WRONG_USAGE;
    }

    CoreSuspender suspend(c);

    for (size_t i = 0; i < world->raws.plants.all.size(); i++)
    {
        df::plant_raw *plant = world->raws.plants.all[i];
        if (plantNames.find(plant->id) != plantNames.end())
        {
            plantNames.erase(plant->id);
            plantIDs.insert(i);
        }
    }
    if (plantNames.size() > 0)
    {
        c->con.printerr("Invalid plant ID(s):");
        for (set<string>::const_iterator it = plantNames.begin(); it != plantNames.end(); it++)
            c->con.printerr(" %s", it->c_str());
        c->con.printerr("\n");
        return CR_FAILURE;
    }

    if (plantIDs.size() == 0)
    {
        c->con.print("Valid plant IDs:\n");
        for (size_t i = 0; i < world->raws.plants.all.size(); i++)
        {
            df::plant_raw *plant = world->raws.plants.all[i];
            if (plant->flags.is_set(plant_raw_flags::GRASS))
                continue;
            c->con.print("* (%s) %s - %s\n", plant->flags.is_set(plant_raw_flags::TREE) ? "tree" : "shrub", plant->id.c_str(), plant->name.c_str());
        }
        return CR_OK;
    }

    count = 0;
    for (size_t i = 0; i < world->map.map_blocks.size(); i++)
    {
        df::map_block *cur = world->map.map_blocks[i];
        bool dirty = false;
        for (size_t j = 0; j < cur->plants.size(); j++)
        {
            const df::plant *plant = cur->plants[j];
            int x = plant->pos.x % 16;
            int y = plant->pos.y % 16;
            if (plantIDs.find(plant->material) != plantIDs.end())
            {
                if (exclude)
                    continue;
            }
            else
            {
                if (!exclude)
                    continue;
            }
            df::tiletype_shape shape = tileShape(cur->tiletype[x][y]);
            df::tiletype_special special = tileSpecial(cur->tiletype[x][y]);
            if (plant->flags.bits.is_shrub && (treesonly || !(shape == tiletype_shape::SHRUB && special != tiletype_special::DEAD)))
                continue;
            if (!plant->flags.bits.is_shrub && (shrubsonly || !(shape == tiletype_shape::TREE)))
                continue;
            if (cur->designation[x][y].bits.hidden)
                continue;
            if (deselect && cur->designation[x][y].bits.dig == tile_dig_designation::Default)
            {
                cur->designation[x][y].bits.dig = tile_dig_designation::No;
                dirty = true;
                ++count;
            }
            if (!deselect && cur->designation[x][y].bits.dig == tile_dig_designation::No)
            {
                cur->designation[x][y].bits.dig = tile_dig_designation::Default;
                dirty = true;
                ++count;
            }
        }
        if (dirty)
            cur->flags.set(block_flags::Designated);
    }
    if (count)
        c->con.print("Updated %d plant designations.\n", count);
    return CR_OK;
}

DFhackCExport const char * plugin_name ( void )
{
    return "getplants";
}

DFhackCExport command_result plugin_init ( Core * c, vector <PluginCommand> &commands)
{
    commands.clear();
    commands.push_back(PluginCommand(
        "getplants", "Cut down all of the specified trees or gather specified shrubs",
        df_getplants, false,
        "  Specify the types of trees to cut down and/or shrubs to gather by their\n"
        "  plant IDs, separated by spaces.\n"
        "Options:\n"
        "  -t - Select trees only (exclude shrubs)\n"
        "  -s - Select shrubs only (exclude trees)\n"
        "  -c - Clear designations instead of setting them\n"
        "  -x - Apply selected action to all plants except those specified\n"
        "Specifying both -t and -s will have no effect.\n"
        "If no plant IDs are specified, all valid plant IDs will be listed.\n"
    ));
    return CR_OK;
}

DFhackCExport command_result plugin_shutdown ( Core * c )
{
    return CR_OK;
}
