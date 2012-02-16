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

/*******************************************************************************
                                    M A P S
                            Read and write DF's map
*******************************************************************************/
#pragma once
#ifndef CL_MOD_MAPS
#define CL_MOD_MAPS

#include "Export.h"
#include "Module.h"
#include "modules/Vegetation.h"
#include <vector>
#include "Virtual.h"
#include "modules/Materials.h"

#include "df/world.h"
#include "df/feature_init.h"
#include "df/map_block.h"
#include "df/block_square_event.h"
#include "df/block_square_event_mineralst.h"
#include "df/block_square_event_frozen_liquidst.h"
#include "df/block_square_event_world_constructionst.h"
#include "df/block_square_event_material_spatterst.h"
#include "df/block_square_event_grassst.h"
#include "df/tile_liquid.h"
#include "df/tile_dig_designation.h"
#include "df/tile_traffic.h"

/**
 * \defgroup grp_maps Maps module and its types
 * @ingroup grp_modules
 */

namespace DFHack
{
/***************************************************************************
                                T Y P E S
***************************************************************************/
/**
    * Function for translating feature index to its name
    * \ingroup grp_maps
    */
extern DFHACK_EXPORT const char * sa_feature(df::feature_type index);

typedef df::coord DFCoord;
typedef DFCoord planecoord;

/**
 * A local or global map feature
 * \ingroup grp_maps
 */
struct t_feature
{
    df::feature_type type;
    /// main material type - decides between stuff like bodily fluids, inorganics, vomit, amber, etc.
    int16_t main_material;
    /// generally some index to a vector of material types.
    int32_t sub_material;
    /// placeholder
    bool discovered;
    /// this is NOT part of the DF feature, but an address of the feature as seen by DFhack.
    df::feature_init * origin;
};

/**
 * \ingroup grp_maps
 */
enum BiomeOffset
{
    eNorthWest,
    eNorth,
    eNorthEast,
    eWest,
    eHere,
    eEast,
    eSouthWest,
    eSouth,
    eSouthEast,
    eBiomeCount
};

/**
 * map block flags
 * \ingroup grp_maps
 */
struct naked_blockflags
{
    /// designated for jobs (digging and stuff like that)
    unsigned int designated : 1;
    /// possibly related to the designated flag
    unsigned int unk_1 : 1;
    /// two flags required for liquid flow.
    unsigned int liquid_1 : 1;
    unsigned int liquid_2 : 1;
    /// rest of the flags is completely unknown
    unsigned int unk_2: 4;
};
/**
 * map block flags wrapper
 * \ingroup grp_maps
 */
union t_blockflags
{
    uint32_t whole;
    naked_blockflags bits;
};

/**
 * 16x16 array of tile types
 * \ingroup grp_maps
 */
typedef df::tiletype tiletypes40d [16][16];
/**
 * 16x16 array used for squashed block materials
 * \ingroup grp_maps
 */
typedef int16_t t_blockmaterials [16][16];
/**
 * 16x16 array of designation flags
 * \ingroup grp_maps
 */
typedef df::tile_designation t_designation;
typedef t_designation designations40d [16][16];
/**
 * 16x16 array of occupancy flags
 * \ingroup grp_maps
 */
typedef df::tile_occupancy t_occupancy;
typedef t_occupancy occupancies40d [16][16];
/**
 * array of 16 biome indexes valid for the block
 * \ingroup grp_maps
 */
typedef uint8_t biome_indices40d [9];
/**
 * 16x16 array of temperatures
 * \ingroup grp_maps
 */
typedef uint16_t t_temperatures [16][16];
/**
 * structure for holding whole blocks
 * \ingroup grp_maps
 */
typedef struct
{
    DFCoord position;
    /// type of the tiles
    tiletypes40d tiletypes;
    /// flags determining the state of the tiles
    designations40d designation;
    /// flags determining what's on the tiles
    occupancies40d occupancy;
    /// values used for geology/biome assignment
    biome_indices40d biome_indices;
    /// the address where the block came from
    df::map_block * origin;
    t_blockflags blockflags;
    /// index into the global feature vector
    int32_t global_feature;
    /// index into the local feature vector... complicated
    int32_t local_feature;
    int32_t mystery;
} mapblock40d;

/**
 * The Maps module
 * \ingroup grp_modules
 * \ingroup grp_maps
 */
namespace Simple
{
namespace Maps
{

extern DFHACK_EXPORT bool IsValid();

/**
 * Method for reading the geological surrounding of the currently loaded region.
 * assign is a reference to an array of nine vectors of unsigned words that are to be filled with the data
 * array is indexed by the BiomeOffset enum
 * 
 * I omitted resolving the layer matgloss in this API, because it would
 * introduce overhead by calling some method for each tile. You have to do it
 * yourself.
 *
 * First get the stuff from ReadGeology and then for each block get the RegionOffsets.
 * For each tile get the real region from RegionOffsets and cross-reference it with
 * the geology stuff (region -- array of vectors, depth -- vector).
 * I'm thinking about turning that Geology stuff into a two-dimensional array
 * with static size.
 *
 * this is the algorithm for applying matgloss:
 * @code

void DfMap::applyGeoMatgloss(Block * b)
{
    // load layer matgloss
    for(int x_b = 0; x_b < BLOCK_SIZE; x_b++)
    {
        for(int y_b = 0; y_b < BLOCK_SIZE; y_b++)
        {
            int geolayer = b->designation[x_b][y_b].bits.geolayer_index;
            int biome = b->designation[x_b][y_b].bits.biome;
            b->material[x_b][y_b].type = Mat_Stone;
            b->material[x_b][y_b].index = v_geology[b->RegionOffsets[biome]][geolayer];
        }
    }
}

 * @endcode
 */
extern DFHACK_EXPORT bool ReadGeology( std::vector < std::vector <uint16_t> >& assign );

/**
 * Get the feature indexes of a block
 */
extern DFHACK_EXPORT bool ReadFeatures(uint32_t x, uint32_t y, uint32_t z, int32_t & local, int32_t & global);
/**
 * Set the feature indexes of a block
 */
extern DFHACK_EXPORT bool WriteFeatures(uint32_t x, uint32_t y, uint32_t z, const int32_t & local, const int32_t & global);
/**
 * Get pointers to features of a block
 */
extern DFHACK_EXPORT bool ReadFeatures(uint32_t x, uint32_t y, uint32_t z, t_feature * local, t_feature * global);
/**
 * Get pointers to features of an already read block
 */
extern DFHACK_EXPORT bool ReadFeatures(mapblock40d * block,t_feature * local, t_feature * global);

/**
 * Read a specific global or local feature directly
 */
extern DFHACK_EXPORT bool GetGlobalFeature(t_feature &feature, int32_t index);
extern DFHACK_EXPORT bool GetLocalFeature(t_feature &feature, df::coord2d coord, int32_t index);

/*
 * BLOCK DATA
 */

/// get size of the map in tiles
extern DFHACK_EXPORT void getSize(uint32_t& x, uint32_t& y, uint32_t& z);
/// get the position of the map on world map
extern DFHACK_EXPORT void getPosition(int32_t& x, int32_t& y, int32_t& z);

/**
 * Get the map block or NULL if block is not valid
 */
extern DFHACK_EXPORT df::map_block * getBlock (int32_t blockx, int32_t blocky, int32_t blockz);
extern DFHACK_EXPORT df::map_block * getBlockAbs (int32_t x, int32_t y, int32_t z);

/// copy the whole map block at block coords (see DFTypes.h for the block structure)
extern DFHACK_EXPORT bool ReadBlock40d(uint32_t blockx, uint32_t blocky, uint32_t blockz, mapblock40d * buffer);

/// copy/write block tile types
extern DFHACK_EXPORT bool ReadTileTypes(uint32_t blockx, uint32_t blocky, uint32_t blockz, tiletypes40d *buffer);
extern DFHACK_EXPORT bool WriteTileTypes(uint32_t blockx, uint32_t blocky, uint32_t blockz, tiletypes40d *buffer);

/// copy/write block designations
extern DFHACK_EXPORT bool ReadDesignations(uint32_t blockx, uint32_t blocky, uint32_t blockz, designations40d *buffer);
extern DFHACK_EXPORT bool WriteDesignations (uint32_t blockx, uint32_t blocky, uint32_t blockz, designations40d *buffer);

/// copy/write temperatures
extern DFHACK_EXPORT bool ReadTemperatures(uint32_t blockx, uint32_t blocky, uint32_t blockz, t_temperatures *temp1, t_temperatures *temp2);
extern DFHACK_EXPORT bool WriteTemperatures (uint32_t blockx, uint32_t blocky, uint32_t blockz, t_temperatures *temp1, t_temperatures *temp2);

/// copy/write block occupancies
extern DFHACK_EXPORT bool ReadOccupancy(uint32_t blockx, uint32_t blocky, uint32_t blockz, occupancies40d *buffer);
extern DFHACK_EXPORT bool WriteOccupancy(uint32_t blockx, uint32_t blocky, uint32_t blockz, occupancies40d *buffer);

/// copy/write the block dirty bit - this is used to mark a map block so that DF scans it for designated jobs like digging
extern DFHACK_EXPORT bool ReadDirtyBit(uint32_t blockx, uint32_t blocky, uint32_t blockz, bool &dirtybit);
extern DFHACK_EXPORT bool WriteDirtyBit(uint32_t blockx, uint32_t blocky, uint32_t blockz, bool dirtybit);

/// copy/write the block flags
extern DFHACK_EXPORT bool ReadBlockFlags(uint32_t blockx, uint32_t blocky, uint32_t blockz, t_blockflags &blockflags);
extern DFHACK_EXPORT bool WriteBlockFlags(uint32_t blockx, uint32_t blocky, uint32_t blockz, t_blockflags blockflags);

/// copy/write features
extern DFHACK_EXPORT bool SetBlockLocalFeature(uint32_t blockx, uint32_t blocky, uint32_t blockz, int32_t local = -1);
extern DFHACK_EXPORT bool SetBlockGlobalFeature(uint32_t blockx, uint32_t blocky, uint32_t blockz, int32_t global = -1);

/// copy region offsets of a block - used for determining layer stone matgloss
extern DFHACK_EXPORT bool ReadRegionOffsets(uint32_t blockx, uint32_t blocky, uint32_t blockz, biome_indices40d *buffer);

/// sorts the block event vector into multiple vectors by type
/// mineral veins, what's under ice, blood smears and mud
extern DFHACK_EXPORT bool SortBlockEvents(uint32_t x, uint32_t y, uint32_t z,
    std::vector<df::block_square_event_mineralst *>* veins,
    std::vector<df::block_square_event_frozen_liquidst *>* ices = 0,
    std::vector<df::block_square_event_material_spatterst *>* splatter = 0,
    std::vector<df::block_square_event_grassst *>* grass = 0,
    std::vector<df::block_square_event_world_constructionst *>* constructions = 0
);

/// remove a block event from the block by address
extern DFHACK_EXPORT bool RemoveBlockEvent(uint32_t x, uint32_t y, uint32_t z, df::block_square_event * which );

/// read all plants in this block
extern DFHACK_EXPORT bool ReadVegetation(uint32_t x, uint32_t y, uint32_t z, std::vector<df::plant *>*& plants);

}
}
}
#endif
